//
//  ADLocationManager.h
//  PregnantAssistant
//
//  Created by ruoyi_zhang on 15/7/31.
//  Copyright (c) 2015年 Adding. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <CoreLocation/CoreLocation.h>
#import "ADUserLoction.h"

@interface ADLocationManager : NSObject <CLLocationManagerDelegate>

@property (nonatomic, retain) CLLocationManager *locationManager;
@property (nonatomic, retain) ADUserLoction *location;

+ (ADLocationManager *)shareLocationManager;

- (void)startLocate;
- (void)getUserLocationOnFinish:(void (^)())aRequstBlock;

@end
