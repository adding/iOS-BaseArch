//
//  UIFont+ADFont.m
//  PregnantAssistant
//
//  Created by D on 14-10-8.
//  Copyright (c) 2014年 Adding. All rights reserved.
//

#import "UIFont+ADFont.h"

@implementation UIFont (ADFont)

+ (UIFont *)btn_title_font
{
    return  [UIFont systemFontOfSize:17];
}

+ (UIFont *)momLookCell_title_font
{
    if (iPhone6 || iPhone6Plus) {
        return [UIFont fontWithName:FZLanTingHei_L_GBk size:CellTitlefontSize_6];
    } else {
        return [UIFont fontWithName:FZLanTingHei_L_GBk size:CellTitlefontSize_5];
    }
}

@end
