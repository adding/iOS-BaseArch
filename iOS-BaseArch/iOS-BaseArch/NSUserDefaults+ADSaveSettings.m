//
//  NSUserDefaults+ADSaveSettings.m
//  PregnantAssistant
//
//  Created by D on 15/1/8.
//  Copyright (c) 2015年 Adding. All rights reserved.
//

#import "NSUserDefaults+ADSaveSettings.h"

@implementation NSUserDefaults (ADSaveSettings)

- (void)setFirstLauch:(BOOL)firstLauch
{
    [self setBool:firstLauch forKey:firstLaunchKey];
    [self synchronize];
}

- (BOOL)firstLauch
{
    return [self boolForKey:firstLaunchKey];
}

@end
