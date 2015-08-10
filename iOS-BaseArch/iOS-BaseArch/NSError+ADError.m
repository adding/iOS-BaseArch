//
//  NSError+ADError.m
//  PregnantAssistant
//
//  Created by D on 15/5/13.
//  Copyright (c) 2015年 Adding. All rights reserved.
//

#import "NSError+ADError.h"

@implementation NSError (ADError)

+ (NSError *)notLoginError
{
    NSDictionary *userInfo = @{ NSLocalizedDescriptionKey:@"not login" };
    return [NSError errorWithDomain:ERR_DOMIN code:ERRCODE_NOTLOGIN userInfo:userInfo];
}

+ (NSError *)lessToolError
{
    NSDictionary *userInfo = @{ NSLocalizedDescriptionKey:@"less 6 tool" };
    return [NSError errorWithDomain:ERR_DOMIN code:ERRCODE_LESS_TOOL userInfo:userInfo];
}

@end
