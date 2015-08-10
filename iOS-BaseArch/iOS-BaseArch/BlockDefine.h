//
//  BlockDefine.h
//  iOS-BaseArch
//
//  Created by D on 15/8/10.
//  Copyright (c) 2015年 addinghome. All rights reserved.
//

#pragma once

typedef void (^blockVoidVoid)  (void);
typedef void (^blockVoidId)    (id value1);
typedef void (^blockVoidIdId)  (id value1, id value2);
typedef void (^blockVoidIdIdId)(id value1, id value2, id value3);
typedef id   (^blockIdVoid)    (void);
typedef id   (^blockIdId)      (id value1);
typedef id   (^blockIdIdId)    (id value1, id value2);
typedef id   (^blockIdIdIdId)  (id value1, id value2, id value3);