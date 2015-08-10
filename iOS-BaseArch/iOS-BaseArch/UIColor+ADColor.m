//
//  UIColor+ADColor.m
//  PregnantAssistant
//
//  Created by D on 14-9-15.
//  Copyright (c) 2014年 Adding. All rights reserved.
//

#import "UIColor+ADColor.h"

#define UIColorFromRGB(rgbValue) [UIColor colorWithRed:((float)((rgbValue & 0xFF0000) >> 16))/255.0 green:((float)((rgbValue & 0xFF00) >> 8))/255.0 blue:((float)(rgbValue & 0xFF))/255.0 alpha:1.0]
#define UIColorFromRGBA(rgbValue, alphaValue) \
[UIColor colorWithRed:((float)((rgbValue & 0xFF0000) >> 16))/255.0 green:((float)((rgbValue & 0xFF00) >> 8))/255.0 blue:((float)(rgbValue & 0xFF))/255.0 alpha:alphaValue]

@implementation UIColor (ADColor)

+ (UIColor *)defaultTintColor
{
    return UIColorFromRGB(0xFF536E);
}

+ (UIColor *)darkTintColor
{
    return UIColorFromRGB(0xFF536E);
}

+ (UIColor *)font_LightBrown
{
    return UIColorFromRGB(0xFF536E);
}

+ (UIColor *)bg_brown
{
    return UIColorFromRGB(0xFF536E);
}

+ (UIColor *)separator_line_color
{
    return [UIColor colorWithRed:178/255.0 green:168/255.0 blue:161/255.0 alpha:0.4];
}

@end
