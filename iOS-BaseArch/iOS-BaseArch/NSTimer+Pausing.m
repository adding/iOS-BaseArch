//
//  NSTimer+Pausing.m
//  PregnantAssistant
//
//  Created by D on 14-10-13.
//  Copyright (c) 2014年 Adding. All rights reserved.
//

#import "NSTimer+Pausing.h"

@implementation NSTimer (Pausing)

- (void)pause
{
    // Prevent invalid timers from being paused
    if(![self isValid])
        return;
    
    [self setFireDate:[NSDate distantFuture]];
}

- (void)resume
{
    // Prevent invalid timers from being resumed
    if(![self isValid])
        return;
    
    NSDate *fireDate1 = [NSDate dateWithTimeIntervalSinceNow:0.1];
    // Resume timer
    [self setFireDate:fireDate1];
    
}

@end
