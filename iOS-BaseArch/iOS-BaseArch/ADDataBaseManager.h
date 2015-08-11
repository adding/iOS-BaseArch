//
//  ADDataBaseManager.h
//  
//
//  Created by D on 15/8/11.
//
//

#import <Foundation/Foundation.h>

@interface ADDataBaseManager : NSObject

+ (instancetype)shareManager;

- (void)migrationDataBase;

@end
