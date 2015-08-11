//
//  ADDataBaseManager.m
//  
//
//  Created by D on 15/8/11.
//
//

#import "ADDataBaseManager.h"
#import <Realm/Realm.h>

@implementation ADDataBaseManager

+ (ADDataBaseManager *)shareManager {
    static dispatch_once_t onceToken;
    static ADDataBaseManager *sharedPushManager = nil;
    
    dispatch_once(&onceToken, ^{
        sharedPushManager = [[ADDataBaseManager alloc] init];
    });
    
    return sharedPushManager;
}

#pragma mark - 迁移数据库
- (void)migrationDataBase {
    
    // 特殊数据库的迁移可以 根据 oldSchemaVersion 做操作，Realm 会自动增删 属性和 表
    [RLMRealm setSchemaVersion:2
                forRealmAtPath:[RLMRealm defaultRealmPath]
            withMigrationBlock:^(RLMMigration *migration, uint64_t oldSchemaVersion) {
                // We haven’t migrated anything yet, so oldSchemaVersion == 0
                if (oldSchemaVersion < 2) {
                    // Nothing to do!
                    // Realm will automatically detect new properties and removed properties
                    // And will update the schema on disk automatically
                }
            }];
    
    // now that we have called `setSchemaVersion:withMigrationBlock:`, opening an outdated
    // Realm will automatically perform the migration and opening the Realm will succeed
    [RLMRealm defaultRealm];
}

@end
