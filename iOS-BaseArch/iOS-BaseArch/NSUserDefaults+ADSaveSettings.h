//
//  NSUserDefaults+ADSaveSettings.h
//  PregnantAssistant
//
//  Created by D on 15/1/8.
//  Copyright (c) 2015年 Adding. All rights reserved.
//

#import <Foundation/Foundation.h>

static NSString *firstLaunchKey = @"firstLaunch";

@interface NSUserDefaults (ADSaveSettings)

@property (nonatomic, assign) BOOL firstLauch;

@end
