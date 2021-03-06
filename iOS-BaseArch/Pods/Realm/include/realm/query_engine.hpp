/*************************************************************************
 *
 * REALM CONFIDENTIAL
 * __________________
 *
 *  [2011] - [2012] Realm Inc
 *  All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains
 * the property of Realm Incorporated and its suppliers,
 * if any.  The intellectual and technical concepts contained
 * herein are proprietary to Realm Incorporated
 * and its suppliers and may be covered by U.S. and Foreign Patents,
 * patents in process, and are protected by trade secret or copyright law.
 * Dissemination of this information or reproduction of this material
 * is strictly forbidden unless prior written permission is obtained
 * from Realm Incorporated.
 *
 **************************************************************************/

/*
A query consists of node objects, one for each query condition. Each node contains pointers to all other nodes:

node1        node2         node3
------       -----         -----
node2*       node1*        node1*
node3*       node3*        node2*

The construction of all this takes part in query.cpp. Each node has two important functions:

    aggregate(start, end)
    aggregate_local(start, end)

The aggregate() function executes the aggregate of a query. You can call the method on any of the nodes
(except children nodes of OrNode and SubtableNode) - it has the same behaviour. The function contains
scheduling that calls aggregate_local(start, end) on different nodes with different start/end ranges,
depending on what it finds is most optimal.

The aggregate_local() function contains a tight loop that tests the condition of its own node, and upon match
it tests all other conditions at that index to report a full match or not. It will remain in the tight loop
after a full match.

So a call stack with 2 and 9 being local matches of a node could look like this:

aggregate(0, 10)
    node1->aggregate_local(0, 3)
        node2->find_first_local(2, 3)
        node3->find_first_local(2, 3)
    node3->aggregate_local(3, 10)
        node1->find_first_local(4, 5)
        node2->find_first_local(4, 5)
        node1->find_first_local(7, 8)
        node2->find_first_local(7, 8)

find_first_local(n, n + 1) is a function that can be used to test a single row of another condition. Note that
this is very simplified. There are other statistical arguments to the methods, and also, find_first_local() can be
called from a callback function called by an integer Array.


Template arguments in methods:
----------------------------------------------------------------------------------------------------

TConditionFunction: Each node has a condition from query_conditions.c such as Equal, GreaterEqual, etc

TConditionValue:    Type of values in condition column. That is, int64_t, float, int, bool, etc

TAction:            What to do with each search result, from the enums act_ReturnFirst, act_Count, act_Sum, etc

TResult:            Type of result of actions - float, double, int64_t, etc. Special notes: For act_Count it's
                    int64_t, for RLM_FIND_ALL it's int64_t which points at destination array.

TSourceColumn:      Type of source column used in actions, or *ignored* if no source column is used (like for
                    act_Count, act_ReturnFirst)


There are two important classes used in queries:
----------------------------------------------------------------------------------------------------
SequentialGetter    Column iterator used to get successive values with leaf caching. Used both for condition columns
                    and aggregate source column

AggregateState      State of the aggregate - contains a state variable that stores intermediate sum, max, min,
                    etc, etc.

*/

#ifndef REALM_QUERY_ENGINE_HPP
#define REALM_QUERY_ENGINE_HPP

#include <string>
#include <functional>
#include <algorithm>

#include <realm/util/shared_ptr.hpp>
#include <realm/util/meta.hpp>
#include <realm/unicode.hpp>
#include <realm/utilities.hpp>
#include <realm/table.hpp>
#include <realm/table_view.hpp>
#include <realm/column_fwd.hpp>
#include <realm/column_string.hpp>
#include <realm/column_string_enum.hpp>
#include <realm/column_binary.hpp>
#include <realm/column_basic.hpp>
#include <realm/query_conditions.hpp>
#include <realm/array_basic.hpp>
#include <realm/array_string.hpp>
#include <realm/column_linklist.hpp>
#include <realm/column_link.hpp>
#include <realm/link_view.hpp>

#include <iostream>
#include <map>

#if _MSC_FULL_VER >= 160040219
#  include <immintrin.h>
#endif

/*

typedef float __m256 __attribute__ ((__vector_size__ (32),
                     __may_alias__));
typedef long long __m256i __attribute__ ((__vector_size__ (32),
                      __may_alias__));
typedef double __m256d __attribute__ ((__vector_size__ (32),
                       __may_alias__));

*/

namespace realm {

// Number of matches to find in best condition loop before breaking out to probe other conditions. Too low value gives too many
// constant time overheads everywhere in the query engine. Too high value makes it adapt less rapidly to changes in match
// frequencies.
const size_t findlocals = 64;

// Average match distance in linear searches where further increase in distance no longer increases query speed (because time
// spent on handling each match becomes insignificant compared to time spent on the search).
const size_t bestdist = 512;

// Minimum number of matches required in a certain condition before it can be used to compute statistics. Too high value can spent
// too much time in a bad node (with high match frequency). Too low value gives inaccurate statistics.
const size_t probe_matches = 4;

const size_t bitwidth_time_unit = 64;

typedef bool (*CallbackDummy)(int64_t);

template<class T> struct ColumnTypeTraits;

template<> struct ColumnTypeTraits<int64_t> {
    typedef Column column_type;
    typedef ArrayInteger array_type;
    typedef int64_t sum_type;
    static const DataType id = type_Int;
};
template<> struct ColumnTypeTraits<bool> {
    typedef Column column_type;
    typedef ArrayInteger array_type;
    typedef int64_t sum_type;
    static const DataType id = type_Bool;
};
template<> struct ColumnTypeTraits<float> {
    typedef ColumnFloat column_type;
    typedef ArrayFloat array_type;
    typedef double sum_type;
    static const DataType id = type_Float;
};
template<> struct ColumnTypeTraits<double> {
    typedef ColumnDouble column_type;
    typedef ArrayDouble array_type;
    typedef double sum_type;
    static const DataType id = type_Double;
};
template<> struct ColumnTypeTraits<DateTime> {
    typedef Column column_type;
    typedef ArrayInteger array_type;
    typedef int64_t sum_type;
    static const DataType id = type_DateTime;
};

template<> struct ColumnTypeTraits<StringData> {
    typedef Column column_type;
    typedef ArrayInteger array_type;
    typedef int64_t sum_type;
    static const DataType id = type_String;
};

// Only purpose is to return 'double' if and only if source column (T) is float and you're doing a sum (A)
template<class T, Action A> struct ColumnTypeTraitsSum {
    typedef T sum_type;
};

template<> struct ColumnTypeTraitsSum<float, act_Sum> {
    typedef double sum_type;
};


class SequentialGetterBase {
public:
    virtual ~SequentialGetterBase() REALM_NOEXCEPT {}
};

template <class ColType>
class SequentialGetter : public SequentialGetterBase {
public:
    using T = typename ColType::value_type;
    using ArrayType = typename ColType::LeafType;

    SequentialGetter() {}

    SequentialGetter(const Table& table, size_t column_ndx)
    {
        if (column_ndx != not_found)
            m_column = static_cast<const ColType*>(&table.get_column_base(column_ndx));
        init(m_column);
    }

    SequentialGetter(const ColType* column)
    {
        init(column);
    }

    ~SequentialGetter() REALM_NOEXCEPT override {}

    void init(const ColType* column)
    {
        m_array_ptr.reset(); // Explicitly destroy the old one first, because we're reusing the memory.
        m_array_ptr.reset(new(&m_leaf_accessor_storage) ArrayType(column->get_alloc()));
        m_column = column;
        m_leaf_end = 0;
    }

    REALM_FORCEINLINE bool cache_next(size_t index)
    {
        // Return wether or not leaf array has changed (could be useful to know for caller)
        if (index >= m_leaf_end || index < m_leaf_start) {
            typename ColType::LeafInfo leaf { &m_leaf_ptr, m_array_ptr.get() };
            std::size_t ndx_in_leaf;
            m_column->get_leaf(index, ndx_in_leaf, leaf);
            m_leaf_start = index - ndx_in_leaf;
            const size_t leaf_size = m_leaf_ptr->size();
            m_leaf_end = m_leaf_start + leaf_size;
            return true;
        }
        return false;
    }


    REALM_FORCEINLINE T get_next(size_t index)
    {
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4800)   // Disable the Microsoft warning about bool performance issue.
#endif

        cache_next(index);
        T av = m_leaf_ptr->get(index - m_leaf_start);
        return av;

#ifdef _MSC_VER
#pragma warning(pop)
#endif
    }

    size_t local_end(size_t global_end)
    {
        if (global_end > m_leaf_end)
            return m_leaf_end - m_leaf_start;
        else
            return global_end - m_leaf_start;
    }

    size_t m_leaf_start;
    size_t m_leaf_end;
    const ColType* m_column;

    const ArrayType* m_leaf_ptr = nullptr;
private:
    // Leaf cache for when the root of the column is not a leaf.
    // This dog and pony show is because Array has a reference to Allocator internally,
    // but we need to be able to transfer queries between contexts, so init() reinitializes
    // the leaf cache in the context of the current column.
    typename std::aligned_storage<sizeof(ArrayType), alignof(ArrayType)>::type m_leaf_accessor_storage;
    std::unique_ptr<ArrayType, PlacementDelete> m_array_ptr;
};


class ParentNode {
    typedef ParentNode ThisType;
public:

    ParentNode(): m_table(0)
    {
    }

    void gather_children(std::vector<ParentNode*>& v)
    {
        m_children.clear();
        ParentNode* p = this;
        size_t i = v.size();
        v.push_back(this);
        p = p->child_criteria();

        if (p)
            p->gather_children(v);

        m_children = v;
        m_children.erase(m_children.begin() + i);
        m_children.insert(m_children.begin(), this);

        m_conds = m_children.size();
    }

    struct score_compare {
        bool operator ()(const ParentNode* a, const ParentNode* b) const { return a->cost() < b->cost(); }
    };

    double cost() const
    {
        return 8 * bitwidth_time_unit / m_dD + m_dT; // dt = 1/64 to 1. Match dist is 8 times more important than bitwidth
    }

    size_t find_first(size_t start, size_t end);

    virtual ~ParentNode() REALM_NOEXCEPT {}

    virtual void init(const Table& table)
    {
        m_table = &table;
        if (m_child)
            m_child->init(table);
        m_column_action_specializer = nullptr;
    }

    virtual bool is_initialized() const
    {
        return m_table != nullptr;
    }

    virtual size_t find_first_local(size_t start, size_t end) = 0;

    virtual ParentNode* child_criteria()
    {
        return m_child;
    }

    virtual void aggregate_local_prepare(Action TAction, DataType col_id);

    template<Action TAction, class TSourceColumn>
    bool column_action_specialization(QueryStateBase* st, SequentialGetterBase* source_column, size_t r)
    {
        // TResult: type of query result
        // TSourceValue: type of aggregate source
        using TSourceValue = typename TSourceColumn::value_type;
        using TResult = typename ColumnTypeTraitsSum<TSourceValue, TAction>::sum_type;

        // Sum of float column must accumulate in double
        REALM_STATIC_ASSERT( !(TAction == act_Sum && (std::is_same<TSourceColumn, float>::value &&
                                                        !std::is_same<TResult, double>::value)), "");

        TSourceValue av{};
        // uses_val test because compiler cannot see that Column::get has no side effect and result is discarded
        if (static_cast<QueryState<TResult>*>(st)->template uses_val<TAction>() && source_column != nullptr) {
            REALM_ASSERT_DEBUG(dynamic_cast<SequentialGetter<TSourceColumn>*>(source_column) != nullptr);
            av = static_cast<SequentialGetter<TSourceColumn>*>(source_column)->get_next(r);
        }
        REALM_ASSERT_DEBUG(dynamic_cast<QueryState<TResult>*>(st) != nullptr);
        bool cont = static_cast<QueryState<TResult>*>(st)->template match<TAction, 0>(r, 0, TResult(av));
        return cont;
    }

    virtual size_t aggregate_local(QueryStateBase* st, size_t start, size_t end, size_t local_limit,
                                   SequentialGetterBase* source_column);


    virtual std::string validate()
    {
        if (error_code != "")
            return error_code;
        if (m_child == nullptr)
            return "";
        else
            return m_child->validate();
    }

    ParentNode(const ParentNode& from)
    {
        m_child = from.m_child;
        m_children = from.m_children;
        m_condition_column_idx = from.m_condition_column_idx;
        m_conds = from.m_conds;
        m_dD = from.m_dD;
        m_dT = from.m_dT;
        m_probes = from.m_probes;
        m_matches = from.m_matches;
    }

    virtual ParentNode* clone() = 0;

    virtual void translate_pointers(const std::map<ParentNode*, ParentNode*>& mapping)
    {
        m_child = mapping.find(m_child)->second;
        for (size_t i = 0; i < m_children.size(); ++i)
            m_children[i] = mapping.find(m_children[i])->second;
    }


    ParentNode* m_child;
    std::vector<ParentNode*>m_children;
    size_t m_condition_column_idx; // Column of search criteria

    size_t m_conds;
    double m_dD; // Average row distance between each local match at current position
    double m_dT; // Time overhead of testing index i + 1 if we have just tested index i. > 1 for linear scans, 0 for index/tableview

    size_t m_probes;
    size_t m_matches;


protected:
    typedef bool (ParentNode::* TColumn_action_specialized)(QueryStateBase*, SequentialGetterBase*, size_t);
    TColumn_action_specialized m_column_action_specializer;
    const Table* m_table;
    std::string error_code;

    const ColumnBase& get_column_base(const Table& table, std::size_t ndx)
    {
        return table.get_column_base(ndx);
    }

    ColumnType get_real_column_type(const Table& table, std::size_t ndx)
    {
        return table.get_real_column_type(ndx);
    }
};

// Used for performing queries on a Tableview. This is done by simply passing the TableView to this query condition
class ListviewNode: public ParentNode {
public:
    ListviewNode(TableView& tv) : m_max(0), m_next(0), m_size(tv.size()), m_tv(tv) { m_child = 0; m_dT = 0.0; }
    ~ListviewNode() REALM_NOEXCEPT override {  }

    // Return the n'th table row index contained in the TableView.
    size_t tableindex(size_t n)
    {
        return to_size_t(m_tv.m_row_indexes.get(n));
    }

    void init(const Table& table) override
    {
        m_table = &table;

        m_dD = m_table->size() / (m_tv.size() + 1.0);
        m_probes = 0;
        m_matches = 0;

        m_next = 0;
        if (m_size > 0)
            m_max = tableindex(m_size - 1);
        if (m_child) m_child->init(table);
    }

    size_t find_first_local(size_t start, size_t end)  override
    {
        // Simply return index of first table row which is >= start
        size_t r;
        r = m_tv.m_row_indexes.find_gte(start, m_next);

        if (r >= end)
            return not_found;

        m_next = r;
        return tableindex(r);
    }

    ParentNode* clone() override
    {
        return new ListviewNode(*this);
    }

    ListviewNode(const ListviewNode& from)
        : ParentNode(from), m_tv(from.m_tv)
    {
        m_max = from.m_max;
        m_next = from.m_next;
        m_size = from.m_size;
        m_child = from.m_child;
    }

protected:
    size_t m_max;
    size_t m_next;
    size_t m_size;

    TableView& m_tv;
};

// For conditions on a subtable (encapsulated in subtable()...end_subtable()). These return the parent row as match if and
// only if one or more subtable rows match the condition.
class SubtableNode: public ParentNode {
public:
    SubtableNode(size_t column): m_column(column) {m_child = 0; m_child2 = 0; m_dT = 100.0;}
    SubtableNode() {};
    ~SubtableNode() REALM_NOEXCEPT override {}

    void init(const Table& table) override
    {
        m_dD = 10.0;
        m_probes = 0;
        m_matches = 0;

        m_table = &table;

        // m_child is first node in condition of subtable query.
        if (m_child) {
            // Can't call init() here as usual since the subtable can be degenerate
            // m_child->init(table);
            std::vector<ParentNode*> v;
            m_child->gather_children(v);
        }

        // m_child2 is next node of parent query
        if (m_child2)
            m_child2->init(table);
    }

    std::string validate() override
    {
        if (error_code != "")
            return error_code;
        if (m_child == nullptr)
            return "Unbalanced subtable/end_subtable block";
        else
            return m_child->validate();
    }

    size_t find_first_local(size_t start, size_t end) override
    {
        REALM_ASSERT(m_table);
        REALM_ASSERT(m_child);

        for (size_t s = start; s < end; ++s) {
            ConstTableRef subtable = m_table->get_subtable(m_column, s);

            if (subtable->is_degenerate())
                return not_found;

            m_child->init(*subtable);
            const size_t subsize = subtable->size();
            const size_t sub = m_child->find_first(0, subsize);

            if (sub != not_found)
                return s;
        }
        return not_found;
    }

    ParentNode* child_criteria() override
    {
        return m_child2;
    }

    ParentNode* clone() override
    {
        return new SubtableNode(*this);
    }

    void translate_pointers(const std::map<ParentNode*, ParentNode*>& mapping) override
    {
        ParentNode::translate_pointers(mapping);
        m_child2 = mapping.find(m_child2)->second;
    }

    SubtableNode(const SubtableNode& from)
        : ParentNode(from)
    {
        m_child2 = from.m_child2;
        m_column = from.m_column;
        m_child = from.m_child;
    }

    ParentNode* m_child2;
    size_t m_column;
};


class IntegerNodeBase : public ParentNode
{
public:
    // This function is called from Array::find() for each search result if TAction == act_CallbackIdx
    // in the IntegerNode::aggregate_local() call. Used if aggregate source column is different from search criteria column
    // Return value: false means that the query-state (which consumes matches) has signalled to stop searching, perhaps
    template <Action TAction, class ColType> bool match_callback(int64_t v)
    {
        using TSourceValue = typename ColType::value_type;
        using QueryStateType = typename ColumnTypeTraitsSum<TSourceValue, TAction>::sum_type;

        size_t i = to_size_t(v);
        m_last_local_match = i;
        m_local_matches++;

        QueryState<QueryStateType>* state = static_cast<QueryState<QueryStateType>*>(m_state);
        SequentialGetter<ColType>* source_column = static_cast<SequentialGetter<ColType>*>(m_source_column);

        // Test remaining sub conditions of this node. m_children[0] is the node that called match_callback(), so skip it
        for (size_t c = 1; c < m_conds; c++) {
            m_children[c]->m_probes++;
            size_t m = m_children[c]->find_first_local(i, i + 1);
            if (m != i)
                return true;
        }

        bool b;
        if (state->template uses_val<TAction>())    { // Compiler cannot see that Column::Get has no side effect and result is discarded
            TSourceValue av = source_column->get_next(i);
            b = state->template match<TAction, false>(i, 0, av);
        }
        else {
            b = state->template match<TAction, false>(i, 0, TSourceValue{});
        }

        return b;
    }

    IntegerNodeBase()
    {
        m_child = 0;
        m_conds = 0;
        m_dT = 1.0 / 4.0;
        m_probes = 0;
        m_matches = 0;
    }

    IntegerNodeBase(const IntegerNodeBase& from) : ParentNode(from)
    {
        // state is transient/only valid during search, no need to copy
        m_child = from.m_child;
        m_conds = 0;
        m_dT = 1.0 / 4.0;
        m_probes = 0;
        m_matches = 0;
    }

    void init(const Table& table) override
    {
        ParentNode::init(table);
        m_array_ptr.reset(); // Explicitly destroy the old one first, because we're reusing the memory.
        m_array_ptr.reset(new(&m_leaf_accessor_storage) ArrayInteger(table.get_alloc()));
    }

    size_t m_last_local_match;
    const ArrayInteger* m_leaf_ptr = nullptr;
    size_t m_leaf_start;
    size_t m_leaf_end;
    size_t m_local_end;

    size_t m_local_matches;
    size_t m_local_limit;
    bool m_fastmode_disabled;
    Action m_TAction;

    QueryStateBase* m_state;
    SequentialGetterBase* m_source_column; // Column of values used in aggregate (act_FindAll, act_ReturnFirst, act_Sum, etc)

    void get_leaf(const Column& col, std::size_t ndx)
    {
        std::size_t ndx_in_leaf;
        Column::LeafInfo leaf_info{&m_leaf_ptr, m_array_ptr.get()};
        col.get_leaf(ndx, ndx_in_leaf, leaf_info);
        m_leaf_start = ndx - ndx_in_leaf;
        m_leaf_end = m_leaf_start + m_leaf_ptr->size();
    }

private:
    std::aligned_storage<sizeof(ArrayInteger), alignof(ArrayInteger)>::type m_leaf_accessor_storage;
    std::unique_ptr<ArrayInteger, PlacementDelete> m_array_ptr;
};

// IntegerNode is for conditions for types stored as integers in a realm::Array (int, date, bool).
//
// We don't yet have any integer indexes (only for strings), but when we get one, we should specialize it
// like: template <class TConditionValue, class Equal> class IntegerNode: public ParentNode
template <class TConditionValue, class TConditionFunction> class IntegerNode: public IntegerNodeBase {
    typedef IntegerNode<TConditionValue, TConditionFunction> ThisType;
public:
    typedef typename ColumnTypeTraits<TConditionValue>::column_type ColType;

    IntegerNode(TConditionValue v, size_t column) : m_value(v), m_find_callback_specialized(nullptr)
    {
        m_condition_column_idx = column;
    }
    ~IntegerNode() REALM_NOEXCEPT override {}

    void init(const Table& table) override
    {
        IntegerNodeBase::init(table);
        m_dD = 100.0;
        m_condition_column = static_cast<const ColType*>(&get_column_base(table, m_condition_column_idx));
        m_table = &table;
        m_leaf_end = 0;
        if (m_child)
            m_child->init(table);
    }

    void aggregate_local_prepare(Action TAction, DataType col_id) override
    {
        m_fastmode_disabled = (col_id == type_Float || col_id == type_Double);
        m_TAction = TAction;

        if (TAction == act_ReturnFirst)
            m_find_callback_specialized = &ThisType::template find_callback_specialization<act_ReturnFirst, Column>;

        else if (TAction == act_Count)
            m_find_callback_specialized = & ThisType::template find_callback_specialization<act_Count, Column>;

        else if (TAction == act_Sum && col_id == type_Int)
            m_find_callback_specialized = & ThisType::template find_callback_specialization<act_Sum, Column>;
        else if (TAction == act_Sum && col_id == type_Float)
            m_find_callback_specialized = & ThisType::template find_callback_specialization<act_Sum, BasicColumn<float>>;
        else if (TAction == act_Sum && col_id == type_Double)
            m_find_callback_specialized = & ThisType::template find_callback_specialization<act_Sum, BasicColumn<double>>;

        else if (TAction == act_Max && col_id == type_Int)
            m_find_callback_specialized = & ThisType::template find_callback_specialization<act_Max, Column>;
        else if (TAction == act_Max && col_id == type_Float)
            m_find_callback_specialized = & ThisType::template find_callback_specialization<act_Max, BasicColumn<float>>;
        else if (TAction == act_Max && col_id == type_Double)
            m_find_callback_specialized = & ThisType::template find_callback_specialization<act_Max, BasicColumn<double>>;

        else if (TAction == act_Min && col_id == type_Int)
            m_find_callback_specialized = & ThisType::template find_callback_specialization<act_Min, Column>;
        else if (TAction == act_Min && col_id == type_Float)
            m_find_callback_specialized = & ThisType::template find_callback_specialization<act_Min, BasicColumn<float>>;
        else if (TAction == act_Min && col_id == type_Double)
            m_find_callback_specialized = & ThisType::template find_callback_specialization<act_Min, BasicColumn<double>>;

        else if (TAction == act_FindAll)
            m_find_callback_specialized = & ThisType::template find_callback_specialization<act_FindAll, Column>;

        else if (TAction == act_CallbackIdx)
            m_find_callback_specialized = & ThisType::template find_callback_specialization<act_CallbackIdx, Column>;

        else {
            REALM_ASSERT(false);
        }
    }

    template <Action TAction, class ColType>
    bool find_callback_specialization(size_t s, size_t end2)
    {
        bool cont = m_leaf_ptr->find<TConditionFunction, act_CallbackIdx>
            (m_value, s - m_leaf_start, end2, m_leaf_start, nullptr,
             std::bind1st(std::mem_fun(&IntegerNodeBase::template match_callback<TAction, ColType>), this));
        return cont;
    }

    // FIXME: should be possible to move this up to IntegerNodeBase...
    size_t aggregate_local(QueryStateBase* st, size_t start, size_t end, size_t local_limit,
                           SequentialGetterBase* source_column) override
    {
        REALM_ASSERT(m_conds > 0);
        int c = TConditionFunction::condition;
        m_local_matches = 0;
        m_local_limit = local_limit;
        m_last_local_match = start - 1;
        m_state = st;

        // If there are no other nodes than us (m_conds == 1) AND the column used for our condition is
        // the same as the column used for the aggregate action, then the entire query can run within scope of that
        // column only, with no references to other columns:
        bool fastmode = (m_conds == 1 &&
                         (source_column == nullptr ||
                          (!m_fastmode_disabled
                           && static_cast<SequentialGetter<ColType>*>(source_column)->m_column == m_condition_column)));
        for (size_t s = start; s < end; ) {
            // Cache internal leaves
            if (s >= m_leaf_end || s < m_leaf_start) {
                get_leaf(*m_condition_column, s);
                size_t w = m_leaf_ptr->get_width();
                m_dT = (w == 0 ? 1.0 / REALM_MAX_BPNODE_SIZE : w / float(bitwidth_time_unit));
            }

            size_t end2;
            if (end > m_leaf_end)
                end2 = m_leaf_end - m_leaf_start;
            else
                end2 = end - m_leaf_start;

            if (fastmode) {
                bool cont = m_leaf_ptr->find(c, m_TAction, m_value, s - m_leaf_start, end2, m_leaf_start, static_cast<QueryState<int64_t>*>(st));
                if (!cont)
                    return not_found;
            }
            // Else, for each match in this node, call our IntegerNode::match_callback to test remaining nodes and/or extract
            // aggregate payload from aggregate column:
            else {
                m_source_column = source_column;
                bool cont = (this->* m_find_callback_specialized)(s, end2);
                if (!cont)
                    return not_found;
            }

            if (m_local_matches == m_local_limit)
                break;

            s = end2 + m_leaf_start;
        }

        if (m_local_matches == m_local_limit) {
            m_dD = (m_last_local_match + 1 - start) / (m_local_matches + 1.0);
            return m_last_local_match + 1;
        }
        else {
            m_dD = (end - start) / (m_local_matches + 1.0);
            return end;
        }
    }

    size_t find_first_local(size_t start, size_t end) override
    {
        TConditionFunction condition;
        REALM_ASSERT(m_table);

        while (start < end) {

            // Cache internal leaves
            if (start >= m_leaf_end || start < m_leaf_start) {
                get_leaf(*m_condition_column, start);
            }

            // Do search directly on cached leaf array
            if (start + 1 == end) {
                if (condition(m_leaf_ptr->get(start - m_leaf_start), m_value))
                    return start;
                else
                    return not_found;
            }

            size_t end2;
            if (end > m_leaf_end)
                end2 = m_leaf_end - m_leaf_start;
            else
                end2 = end - m_leaf_start;

            size_t s = m_leaf_ptr->find_first<TConditionFunction>(m_value, start - m_leaf_start, end2);

            if (s == not_found) {
                start = m_leaf_end;
                continue;
            }
            else
                return s + m_leaf_start;
        }

        return not_found;
    }

    ParentNode* clone() override
    {
        return new IntegerNode<TConditionValue, TConditionFunction>(*this);
    }

    IntegerNode(const IntegerNode& from)
        : IntegerNodeBase(from)
    {
        m_value = from.m_value;
        m_condition_column = from.m_condition_column;
        m_find_callback_specialized = from.m_find_callback_specialized;
        m_child = from.m_child;
    }

    TConditionValue m_value;

protected:
    typedef bool (ThisType::* TFind_callback_specialised)(size_t, size_t);

    const ColType* m_condition_column;                // Column on which search criteria is applied
    TFind_callback_specialised m_find_callback_specialized;
};




// This node is currently used for floats and doubles only
template <class ColType, class TConditionFunction> class FloatDoubleNode: public ParentNode {
public:
    using TConditionValue = typename ColType::value_type;

    FloatDoubleNode(TConditionValue v, size_t column_ndx) : m_value(v)
    {
        m_condition_column_idx = column_ndx;
        m_child = nullptr;
        m_dT = 1.0;
    }
    ~FloatDoubleNode() REALM_NOEXCEPT override {}

    void init(const Table& table) override
    {
        m_dD = 100.0;
        m_table = &table;
        m_condition_column.init(static_cast<const ColType*>(&get_column_base(table, m_condition_column_idx)));

        if (m_child)
            m_child->init(table);
    }

    size_t find_first_local(size_t start, size_t end) override
    {
        TConditionFunction cond;

        for (size_t s = start; s < end; ++s) {
            TConditionValue v = m_condition_column.get_next(s);
            if (cond(v, m_value))
                return s;
        }
        return not_found;
    }


    ParentNode* clone() override
    {
        return new FloatDoubleNode(*this);
    }

    FloatDoubleNode(const FloatDoubleNode& from)
        : ParentNode(from)
    {
        m_value = from.m_value;
        m_child = from.m_child;
        // m_condition_column is not copied
    }

protected:
    TConditionValue m_value;
    SequentialGetter<ColType> m_condition_column;
};


template <class TConditionFunction> class BinaryNode: public ParentNode {
public:
    template <Action TAction> int64_t find_all(Column* /*res*/, size_t /*start*/, size_t /*end*/, size_t /*limit*/, size_t /*source_column*/) {REALM_ASSERT(false); return 0;}

    BinaryNode(BinaryData v, size_t column)
    {
        m_dT = 100.0;
        m_condition_column_idx = column;
        m_child = nullptr;

        // FIXME: Store this in std::string instead.
        char* data = v.is_null() ? nullptr : new char[v.size()];
        memcpy(data, v.data(), v.size());
        m_value = BinaryData(data, v.size());
    }

    ~BinaryNode() REALM_NOEXCEPT override
    {
        delete[] m_value.data();
    }

    void init(const Table& table) override
    {
        m_dD = 100.0;
        m_table = &table;
        m_condition_column = static_cast<const ColumnBinary*>(&get_column_base(table, m_condition_column_idx));
        m_column_type = get_real_column_type(table, m_condition_column_idx);

        if (m_child)
            m_child->init(table);
    }

    size_t find_first_local(size_t start, size_t end) override
    {
        TConditionFunction condition;
        for (size_t s = start; s < end; ++s) {
            BinaryData value = m_condition_column->get(s);
            if (condition(m_value, value))
                return s;
        }
        return not_found;
    }

    ParentNode* clone() override
    {
        return new BinaryNode(*this);
    }

    BinaryNode(const BinaryNode& from)
        : ParentNode(from)
    {
        // FIXME: Store this in std::string instead.
        char* data = new char[from.m_value.size()];
        memcpy(data, from.m_value.data(), from.m_value.size());
        m_value = BinaryData(data, from.m_value.size());
        m_condition_column = from.m_condition_column;
        m_column_type = from.m_column_type;
        m_child = from.m_child;
    }


protected:
private:
    BinaryData m_value;
protected:
    const ColumnBinary* m_condition_column;
    ColumnType m_column_type;
};


class StringNodeBase : public ParentNode {
public:
    template <Action TAction>
    int64_t find_all(Column*, size_t, size_t, size_t, size_t)
    {
        REALM_ASSERT(false);
        return 0;
    }
    StringNodeBase(StringData v, size_t column)
    {
        m_condition_column_idx = column;
        m_child = nullptr;
        m_dT = 10.0;
        m_leaf = nullptr;

        // FIXME: Store these in std::string instead.
        // '*6' because case converted strings can take up more space. Todo, investigate
        char* data;
        data = v.data() ? new char[6 * v.size()] : nullptr; // FIXME: Arithmetic is prone to overflow
        memcpy(data, v.data(), v.size());
        m_value = StringData(data, v.size());
    }

    ~StringNodeBase() REALM_NOEXCEPT override
    {
        delete[] m_value.data();
    }

    void init(const Table& table) override
    {
        m_probes = 0;
        m_matches = 0;
        m_end_s = 0;
        m_leaf_start = 0;
        m_leaf_end = 0;
        m_table = &table;
        m_condition_column = &get_column_base(table, m_condition_column_idx);
        m_column_type = get_real_column_type(table, m_condition_column_idx);
    }

    void clear_leaf_state()
    {
        m_leaf.reset(nullptr);
    }

    StringNodeBase(const StringNodeBase& from)
        : ParentNode(from)
    {
        char* data = from.m_value.data() ? new char[from.m_value.size()] : nullptr;
        memcpy(data, from.m_value.data(), from.m_value.size());
        m_value = StringData(data, from.m_value.size());
        m_condition_column = from.m_condition_column;
        m_column_type = from.m_column_type;
        m_leaf_type = from.m_leaf_type;
        m_end_s = 0;
        m_leaf_start = 0;
        m_child = from.m_child;
    }

protected:
    StringData m_value;

    const ColumnBase* m_condition_column;
    ColumnType m_column_type;

    // Used for linear scan through short/long-string
    std::unique_ptr<const ArrayParent> m_leaf;
    AdaptiveStringColumn::LeafType m_leaf_type;
    size_t m_end_s;
    size_t m_leaf_start;
    size_t m_leaf_end;

};

// Conditions for strings. Note that Equal is specialized later in this file!
template <class TConditionFunction> class StringNode: public StringNodeBase {
public:
    StringNode(StringData v, size_t column) : StringNodeBase(v,column)
    {
        char* upper = new char[6 * v.size()];
        char* lower = new char[6 * v.size()];

        bool b1 = case_map(v, lower, false);
        bool b2 = case_map(v, upper, true);
        if (!b1 || !b2)
            error_code = "Malformed UTF-8: " + std::string(v);

        m_ucase = upper;
        m_lcase = lower;
    }

    ~StringNode() REALM_NOEXCEPT override
    {
        delete[] m_ucase;
        delete[] m_lcase;

        clear_leaf_state();
    }


    void init(const Table& table) override
    {
        clear_leaf_state();

        m_dD = 100.0;

        StringNodeBase::init(table);

        if (m_child)
            m_child->init(table);
    }


    size_t find_first_local(size_t start, size_t end) override
    {
        TConditionFunction cond;

        for (size_t s = start; s < end; ++s) {
            StringData t;

            if (m_column_type == col_type_StringEnum) {
                // enum
                t = static_cast<const ColumnStringEnum*>(m_condition_column)->get(s);
            }
            else {
                // short or long
                const AdaptiveStringColumn* asc = static_cast<const AdaptiveStringColumn*>(m_condition_column);
                REALM_ASSERT_3(s, <, asc->size());
                if (s >= m_end_s || s < m_leaf_start) {
                    // we exceeded current leaf's range
                    clear_leaf_state();
                    std::size_t ndx_in_leaf;
                    m_leaf = asc->get_leaf(s, ndx_in_leaf, m_leaf_type);
                    m_leaf_start = s - ndx_in_leaf;

                    if (m_leaf_type == AdaptiveStringColumn::leaf_type_Small)
                        m_end_s = m_leaf_start + static_cast<const ArrayString&>(*m_leaf).size();
                    else if (m_leaf_type ==  AdaptiveStringColumn::leaf_type_Medium)
                        m_end_s = m_leaf_start + static_cast<const ArrayStringLong&>(*m_leaf).size();
                    else
                        m_end_s = m_leaf_start + static_cast<const ArrayBigBlobs&>(*m_leaf).size();
                }

                if (m_leaf_type == AdaptiveStringColumn::leaf_type_Small)
                    t = static_cast<const ArrayString&>(*m_leaf).get(s - m_leaf_start);
                else if (m_leaf_type ==  AdaptiveStringColumn::leaf_type_Medium)
                    t = static_cast<const ArrayStringLong&>(*m_leaf).get(s - m_leaf_start);
                else
                    t = static_cast<const ArrayBigBlobs&>(*m_leaf).get_string(s - m_leaf_start);
            }
            if (cond(m_value, m_ucase, m_lcase, t))
                return s;
        }
        return not_found;
    }

    ParentNode* clone() override
    {
        return new StringNode<TConditionFunction>(*this);
    }

    StringNode(const StringNode& from) : StringNodeBase(from)
    {
        size_t sz = 6 * m_value.size();
        char* lcase = new char[sz];
        char* ucase = new char[sz];
        memcpy(lcase, from.m_lcase, sz);
        memcpy(ucase, from.m_ucase, sz);
        m_lcase = lcase;
        m_ucase = ucase;
        m_child = from.m_child;
    }
protected:
    const char* m_lcase;
    const char* m_ucase;
};



// Specialization for Equal condition on Strings - we specialize because we can utilize indexes (if they exist) for Equal.
// Future optimization: make specialization for greater, notequal, etc
template<> class StringNode<Equal>: public StringNodeBase {
public:
    StringNode(StringData v, size_t column): StringNodeBase(v,column)
    {
    }
    ~StringNode() REALM_NOEXCEPT override
    {
        deallocate();
    }

    void deallocate() REALM_NOEXCEPT
    {
        // Must be called after each query execution too free temporary resources used by the execution. Run in
        // destructor, but also in Init because a user could define a query once and execute it multiple times.
        clear_leaf_state();

        if (m_index_matches_destroy)
            m_index_matches->destroy();

        m_index_matches_destroy = false;
        m_index_matches.reset();
        m_index_getter.reset();
    }

    void init(const Table& table) override
    {
        deallocate();
        m_dD = 10.0;
        StringNodeBase::init(table);

        if (m_column_type == col_type_StringEnum) {
            m_dT = 1.0;
            m_key_ndx = static_cast<const ColumnStringEnum*>(m_condition_column)->GetKeyNdx(m_value);
        }
        else if (m_condition_column->has_search_index()) {
            m_dT = 0.0;
        }
        else {
            m_dT = 10.0;
        }

        if (m_condition_column->has_search_index()) {

            FindRes fr;
            size_t index_ref;

            if (m_column_type == col_type_StringEnum) {
                fr = static_cast<const ColumnStringEnum*>(m_condition_column)->find_all_indexref(m_value, index_ref);
            }
            else {
                fr = static_cast<const AdaptiveStringColumn*>(m_condition_column)->find_all_indexref(m_value, index_ref);
            }

            m_index_matches_destroy = false;
            m_last_indexed = 0;
            m_last_start = 0;

            switch (fr) {
                case FindRes_single:
                    m_index_matches.reset(new Column(Column::unattached_root_tag(), Allocator::get_default())); // Throws
                    m_index_matches->get_root_array()->create(Array::type_Normal); // Throws
                    m_index_matches->add(index_ref);
                    m_index_matches_destroy = true;        // we own m_index_matches, so we must destroy it
                    break;

                case FindRes_column:
                    // todo: Apparently we can't use m_index.get_alloc() because it uses default allocator which simply makes
                    // translate(x) = x. Shouldn't it inherit owner column's allocator?!
                    m_index_matches.reset(new Column(Column::unattached_root_tag(), m_condition_column->get_alloc())); // Throws
                    m_index_matches->get_root_array()->init_from_ref(index_ref);
                    break;

                case FindRes_not_found:
                    m_index_matches.reset();
                    m_index_getter.reset();
                    m_index_size = 0;
                    break;
            }

            if (m_index_matches) {
                m_index_getter.reset(new SequentialGetter<Column>(m_index_matches.get()));
                m_index_size = m_index_getter->m_column->size();
            }

        }
        else if (m_column_type != col_type_String) {
            REALM_ASSERT_DEBUG(dynamic_cast<const ColumnStringEnum*>(m_condition_column));
            m_cse.init(static_cast<const ColumnStringEnum*>(m_condition_column));
        }

        if (m_child)
            m_child->init(table);
    }

    size_t find_first_local(size_t start, size_t end) override
    {
        REALM_ASSERT(m_table);

        if (m_condition_column->has_search_index()) {
            // Indexed string column
            if (!m_index_getter)
                return not_found; // no matches in the index

            size_t f = not_found;

            if (m_last_start > start)
                m_last_indexed = 0;
            m_last_start = start;

            while (f == not_found && m_last_indexed < m_index_size) {
                m_index_getter->cache_next(m_last_indexed);
                f = m_index_getter->m_leaf_ptr->find_gte(start, m_last_indexed - m_index_getter->m_leaf_start, nullptr);

                if (f >= end || f == not_found) {
                    m_last_indexed = m_index_getter->m_leaf_end;
                }
                else {
                    start = to_size_t(m_index_getter->m_leaf_ptr->get(f));
                    if (start >= end)
                        return not_found;
                    else {
                        m_last_indexed = f + m_index_getter->m_leaf_start;
                        return start;
                    }
                }
            }
            return not_found;
        }

        if (m_column_type != col_type_String) {
            // Enum string column
            if (m_key_ndx == not_found)
                return not_found;  // not in key set

            for (size_t s = start; s < end; ++s) {
                m_cse.cache_next(s);
                s = m_cse.m_leaf_ptr->find_first(m_key_ndx, s - m_cse.m_leaf_start, m_cse.local_end(end));
                if (s == not_found)
                    s = m_cse.m_leaf_end - 1;
                else
                    return s + m_cse.m_leaf_start;
            }

            return not_found;
        }

        // Normal string column, with long or short leaf
        for (size_t s = start; s < end; ++s) {
            const AdaptiveStringColumn* asc = static_cast<const AdaptiveStringColumn*>(m_condition_column);
            if (s >= m_leaf_end || s < m_leaf_start) {
                clear_leaf_state();
                std::size_t ndx_in_leaf;
                m_leaf = asc->get_leaf(s, ndx_in_leaf, m_leaf_type);
                m_leaf_start = s - ndx_in_leaf;
                if (m_leaf_type == AdaptiveStringColumn::leaf_type_Small)
                    m_leaf_end = m_leaf_start + static_cast<const ArrayString&>(*m_leaf).size();
                else if (m_leaf_type ==  AdaptiveStringColumn::leaf_type_Medium)
                    m_leaf_end = m_leaf_start + static_cast<const ArrayStringLong&>(*m_leaf).size();
                else
                    m_leaf_end = m_leaf_start + static_cast<const ArrayBigBlobs&>(*m_leaf).size();
                REALM_ASSERT(m_leaf);
            }
            size_t end2 = (end > m_leaf_end ? m_leaf_end - m_leaf_start : end - m_leaf_start);

            if (m_leaf_type == AdaptiveStringColumn::leaf_type_Small)
                s = static_cast<const ArrayString&>(*m_leaf).find_first(m_value, s - m_leaf_start, end2);
            else if (m_leaf_type ==  AdaptiveStringColumn::leaf_type_Medium)
                s = static_cast<const ArrayStringLong&>(*m_leaf).find_first(m_value, s - m_leaf_start, end2);
            else
                s = static_cast<const ArrayBigBlobs&>(*m_leaf).find_first(str_to_bin(m_value), true, s - m_leaf_start, end2);

            if (s == not_found)
                s = m_leaf_end - 1;
            else
                return s + m_leaf_start;
        }

        return not_found;
    }

public:
    ParentNode* clone() override
    {
        return new StringNode<Equal>(*this);
    }

    StringNode(const StringNode& from) : StringNodeBase(from)
    {
        m_index_matches_destroy = false;
    }

private:
    inline BinaryData str_to_bin(const StringData& s) REALM_NOEXCEPT
    {
        return BinaryData(s.data(), s.size());
    }

    size_t m_key_ndx = not_found;
    size_t m_last_indexed;

    // Used for linear scan through enum-string
    SequentialGetter<ColumnStringEnum> m_cse;

    // Used for index lookup
    std::unique_ptr<Column> m_index_matches;
    bool m_index_matches_destroy = false;
    std::unique_ptr<SequentialGetter<Column>> m_index_getter;
    size_t m_index_size;
    size_t m_last_start;
};

// OR node contains at least two node pointers: Two or more conditions to OR
// together in m_cond, and the next AND condition (if any) in m_child.
//
// For 'second.equal(23).begin_group().first.equal(111).Or().first.equal(222).end_group().third().equal(555)', this
// will first set m_cond[0] = left-hand-side through constructor, and then later, when .first.equal(222) is invoked,
// invocation will set m_cond[1] = right-hand-side through Query& Query::Or() (see query.cpp). In there, m_child is
// also set to next AND condition (if any exists) following the OR.
class OrNode: public ParentNode {
public:
    template <Action TAction> int64_t find_all(Column*, size_t, size_t, size_t, size_t)
    {
        REALM_ASSERT(false);
        return 0;
    }

    OrNode(ParentNode* p1) : m_cond(1, p1) {
        m_child = nullptr;
        m_dT = 50.0;
    }

    ~OrNode() REALM_NOEXCEPT override {}

    void init(const Table& table) override
    {
        m_dD = 10.0;

        std::vector<ParentNode*> v;

        m_start.clear();
        m_start.resize(m_cond.size(), 0);

        m_last.clear();
        m_last.resize(m_cond.size(), 0);

        m_was_match.clear();
        m_was_match.resize(m_cond.size(), false);

        for (size_t c = 0; c < m_cond.size(); ++c) {
            m_cond[c]->init(table);
            v.clear();
            m_cond[c]->gather_children(v);
        }

        if (m_child)
            m_child->init(table);

        m_table = &table;
    }

    size_t find_first_local(size_t start, size_t end) override
    {
        if (start >= end)
            return not_found;

        size_t index = not_found;

        for (size_t c = 0; c < m_cond.size(); ++c) {
            // out of order search; have to discard cached results
            if (start < m_start[c]) {
                m_last[c] = 0;
                m_was_match[c] = false;
            }
            // already searched this range and didn't match
            else if (m_last[c] >= end)
                continue;
            // already search this range and *did* match
           else if (m_was_match[c] && m_last[c] >= start) {
                if (index > m_last[c])
                    index = m_last[c];
               continue;
            }

            m_start[c] = start;
            size_t fmax = std::max(m_last[c], start);
            size_t f = m_cond[c]->find_first(fmax, end);
            m_was_match[c] = f != not_found;
            m_last[c] = f == not_found ? end : f;
            if (f != not_found && index > m_last[c])
                index = m_last[c];
        }

        return index;
    }

    std::string validate() override
    {
        if (error_code != "")
            return error_code;
        if (m_cond[0] == 0)
            return "Missing left-hand side of OR";
        if (m_cond.back() == 0)
            return "Missing final right-hand side of OR";
        std::string s;
        if (m_child != 0)
            s = m_child->validate();
        if (s != "")
            return s;
        for (size_t i = 0; i < m_cond.size(); ++i) {
            s = m_cond[i]->validate();
            if (s != "")
                return s;
        }
        return "";
    }

    ParentNode* clone() override
    {
        return new OrNode(*this);
    }

    void translate_pointers(const std::map<ParentNode*, ParentNode*>& mapping) override
    {
        ParentNode::translate_pointers(mapping);
        for (size_t i = 0; i < m_cond.size(); ++i)
            m_cond[i] = mapping.find(m_cond[i])->second;
    }

    std::vector<ParentNode*> m_cond;
private:
    // start index of the last find for each cond
    std::vector<size_t> m_start;
    // last looked at index of the lasft find for each cond
    // is a matching index if m_was_match is true
    std::vector<size_t> m_last;
    std::vector<bool> m_was_match;
};



class NotNode: public ParentNode {
public:
    template <Action TAction> int64_t find_all(Column*, size_t, size_t, size_t, size_t)
    {
        REALM_ASSERT(false);
        return 0;
    }

    NotNode() {m_child = nullptr; m_cond = nullptr; m_dT = 50.0;}
    ~NotNode() REALM_NOEXCEPT override {}

    void init(const Table& table) override
    {
        m_dD = 10.0;

        std::vector<ParentNode*> v;

        m_cond->init(table);
        v.clear();
        m_cond->gather_children(v);

        // Heuristics bookkeeping:
        m_known_range_start = 0;
        m_known_range_end = 0;
        m_first_in_known_range = not_found;

        if (m_child)
            m_child->init(table);

        m_table = &table;
    }

    size_t find_first_local(size_t start, size_t end) override;

    std::string validate() override
    {
        if (error_code != "")
            return error_code;
        if (m_cond == 0)
            return "Missing argument to Not";
        std::string s;
        if (m_child != 0)
            s = m_child->validate();
        if (s != "")
            return s;
        s = m_cond->validate();
        if (s != "")
            return s;
        return "";
    }

    ParentNode* clone() override
    {
        return new NotNode(*this);
    }

    void translate_pointers(const std::map<ParentNode*, ParentNode*>& mapping) override
    {
        ParentNode::translate_pointers(mapping);
        m_cond = mapping.find(m_cond)->second;
    }

    NotNode(const NotNode& from)
        : ParentNode(from)
    {
        // here we are just copying the pointers - they'll be remapped by "translate_pointers"
        m_cond = from.m_cond;
        m_known_range_start = from.m_known_range_start;
        m_known_range_end = from.m_known_range_end;
        m_first_in_known_range = from.m_first_in_known_range;
        m_child = from.m_child;
    }

    ParentNode* m_cond;
private:
    // FIXME This heuristic might as well be reused for all condition nodes.
    size_t m_known_range_start;
    size_t m_known_range_end;
    size_t m_first_in_known_range;

    bool evaluate_at(size_t rowndx);
    void update_known(size_t start, size_t end, size_t first);
    size_t find_first_loop(size_t start, size_t end);
    size_t find_first_covers_known(size_t start, size_t end);
    size_t find_first_covered_by_known(size_t start, size_t end);
    size_t find_first_overlap_lower(size_t start, size_t end);
    size_t find_first_overlap_upper(size_t start, size_t end);
    size_t find_first_no_overlap(size_t start, size_t end);
};


// Compare two columns with eachother row-by-row
template <class ColType, class TConditionFunction> class TwoColumnsNode: public ParentNode {
public:
    using TConditionValue = typename ColType::value_type;

    template <Action TAction> int64_t find_all(Column* /*res*/, size_t /*start*/, size_t /*end*/, size_t /*limit*/, size_t /*source_column*/) {REALM_ASSERT(false); return 0;}

    TwoColumnsNode(size_t column1, size_t column2)
    {
        m_dT = 100.0;
        m_condition_column_idx1 = column1;
        m_condition_column_idx2 = column2;
        m_child = nullptr;
    }

    ~TwoColumnsNode() REALM_NOEXCEPT override
    {
        delete[] m_value.data();
    }

    void init(const Table& table) override
    {
        m_dD = 100.0;
        m_table = &table;

        const ColumnBase* cb = &get_column_base(table, m_condition_column_idx1);
        REALM_ASSERT_DEBUG(dynamic_cast<const ColType*>(cb));
        const ColType* c = static_cast<const ColType*>(cb);
        m_getter1.init(c);

        c = static_cast<const ColType*>(&get_column_base(table, m_condition_column_idx2));
        m_getter2.init(c);

        if (m_child)
            m_child->init(table);
    }

    size_t find_first_local(size_t start, size_t end) override
    {
        size_t s = start;

        while (s < end) {
            if (std::is_same<TConditionValue, int64_t>::value) {
                // For int64_t we've created an array intrinsics named CompareLeafs which template expands bitwidths
                // of boths arrays to make Get faster.
                m_getter1.cache_next(s);
                m_getter2.cache_next(s);

                QueryState<int64_t> qs;
                bool resume = m_getter1.m_leaf_ptr->template CompareLeafs<TConditionFunction, act_ReturnFirst>(m_getter2.m_leaf_ptr, s - m_getter1.m_leaf_start, m_getter1.local_end(end), 0, &qs, CallbackDummy());

                if (resume)
                    s = m_getter1.m_leaf_end;
                else
                    return to_size_t(qs.m_state) + m_getter1.m_leaf_start;
            }
            else {
                // This is for float and double.

#if 0 && defined(REALM_COMPILER_AVX)
// AVX has been disabled because of array alignment (see https://app.asana.com/0/search/8836174089724/5763107052506)
//
// For AVX you can call things like if (sseavx<1>()) to test for AVX, and then utilize _mm256_movemask_ps (VC)
// or movemask_cmp_ps (gcc/clang)
//
// See https://github.com/rrrlasse/realm/tree/AVX for an example of utilizing AVX for a two-column search which has
// been benchmarked to: floats: 288 ms vs 552 by using AVX compared to 2-level-unrolled FPU loop. doubles: 415 ms vs
// 475 (more bandwidth bound). Tests against SSE have not been performed; AVX may not pay off. Please benchmark
#endif

                TConditionValue v1 = m_getter1.get_next(s);
                TConditionValue v2 = m_getter2.get_next(s);
                TConditionFunction C;

                if (C(v1, v2))
                    return s;
                else
                    s++;
            }
        }
        return not_found;
    }

    ParentNode* clone() override
    {
        return new TwoColumnsNode<ColType, TConditionFunction>(*this);
    }

    TwoColumnsNode(const TwoColumnsNode& from)
        : ParentNode(from)
    {
        m_value = from.m_value;
        m_condition_column = from.m_condition_column;
        m_column_type = from.m_column_type;
        m_condition_column_idx1 = from.m_condition_column_idx1;
        m_condition_column_idx2 = from.m_condition_column_idx2;
        m_child = from.m_child;
        // NOT copied:
        // m_getter1 = from.m_getter1;
        // m_getter2 = from.m_getter2;
    }

protected:
    BinaryData m_value;
    const ColumnBinary* m_condition_column;
    ColumnType m_column_type;

    size_t m_condition_column_idx1;
    size_t m_condition_column_idx2;

    SequentialGetter<ColType> m_getter1;
    SequentialGetter<ColType> m_getter2;
};

// todo, fixme: move this up! There are just some annoying compiler errors that need to be resolved when doing this
#include "query_expression.hpp"


// For Next-Generation expressions like col1 / col2 + 123 > col4 * 100.
class ExpressionNode: public ParentNode {

public:
    ~ExpressionNode() REALM_NOEXCEPT { }

    ExpressionNode(Expression* compare, bool auto_delete)
    {
        m_auto_delete = auto_delete;
        m_child = nullptr;
        m_compare = util::SharedPtr<Expression>(compare);
        m_dD = 10.0;
        m_dT = 50.0;
    }

    void init(const Table& table)  override
    {
        m_compare->set_table();
        if (m_child)
            m_child->init(table);
    }

    size_t find_first_local(size_t start, size_t end) override
    {
        size_t res = m_compare->find_first(start, end);
        return res;
    }

    ParentNode* clone() override
    {
        return new ExpressionNode(*this);
    }

    ExpressionNode(ExpressionNode& from)
        : ParentNode(from)
    {
        m_compare = from.m_compare;
        m_child = from.m_child;
    }

    bool m_auto_delete;
    util::SharedPtr<Expression> m_compare;
};


class LinksToNode : public ParentNode {
public:
    LinksToNode(size_t origin_column_index, size_t target_row) : m_origin_column(origin_column_index),
                                                                 m_target_row(target_row)
    {
        m_child = nullptr;
        m_dD = 10.0;
        m_dT = 50.0;
    }

    void init(const Table& table) override
    {
        m_table = &table;
        if (m_child)
            m_child->init(table);
    }

    size_t find_first_local(size_t start, size_t end) override
    {
        size_t ret = realm::npos; // superfluous init, but gives warnings otherwise
        DataType type = m_table->get_column_type(m_origin_column);

        if (type == type_Link) {
            ColumnLinkBase& clb = const_cast<Table*>(m_table)->get_column_link_base(m_origin_column);
            ColumnLink& cl = static_cast<ColumnLink&>(clb);
            ret = cl.find_first(m_target_row + 1, start, end); // ColumnLink stores link to row N as the integer N + 1
        }
        else if (type == type_LinkList) {
            ColumnLinkBase& clb = const_cast<Table*>(m_table)->get_column_link_base(m_origin_column);
            ColumnLinkList& cll = static_cast<ColumnLinkList&>(clb);
            for (size_t i = start; i < end; i++) {
                LinkViewRef lv = cll.get(i);
                ret = lv->find(m_target_row);
                if (ret != not_found)
                    return i;
            }
        }
        else {
            REALM_ASSERT(false);
        }

        return ret;
    }

    ParentNode* clone() override
    {
        return new LinksToNode(*this);
    }

    size_t m_origin_column;
    size_t m_target_row;
};


} // namespace realm

#endif // REALM_QUERY_ENGINE_HPP
