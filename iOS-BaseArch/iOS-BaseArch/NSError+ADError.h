//
//  NSError+ADError.h
//  PregnantAssistant
//
//  Created by D on 15/5/13.
//  Copyright (c) 2015年 Adding. All rights reserved.
//

#import <Foundation/Foundation.h>

#define ERRCODE_NOTLOGIN 100
#define ERRCODE_LESS_TOOL 101
#define ERR_DOMIN @"com.addinghome.pa"

@interface NSError (ADError)

+ (NSError *)notLoginError;

+ (NSError *)lessToolError;

@end
