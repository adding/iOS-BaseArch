//
//  ADLocationManager.m
//  PregnantAssistant
//
//  Created by ruoyi_zhang on 15/7/31.
//  Copyright (c) 2015年 Adding. All rights reserved.
//

#import "ADLocationManager.h"
#import <AFNetworking.h>

@implementation ADLocationManager

static ADLocationManager *shareLocationManager = nil;

+ (ADLocationManager *)shareLocationManager
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        shareLocationManager = [[ADLocationManager alloc] init];
        shareLocationManager.location = [[ADUserLoction alloc]init];
    });
    return shareLocationManager;
}

- (void)startLocate
{
    if (!self.locationManager) {
        self.locationManager = [[CLLocationManager alloc] init];
        if (IOS8_OR_LATER)
            [self.locationManager requestWhenInUseAuthorization];
        
        self.locationManager.delegate = self;
    }
    
    [self.locationManager startUpdatingLocation];
}

- (void)locationManager:(CLLocationManager *)manager
    didUpdateToLocation:(CLLocation *)newLocation
           fromLocation:(CLLocation *)oldLocation
{    
    self.location.longitude = @(newLocation.coordinate.longitude).stringValue;
    self.location.latitude = @(newLocation.coordinate.latitude).stringValue;
    
//    [[NSUserDefaults standardUserDefaults] setLongitude:self.location.longitude];
//    [[NSUserDefaults standardUserDefaults] setLatitude:self.location.latitude];
    [manager stopUpdatingLocation];
}

- (void)getUserLocationOnFinish:(void (^)())aRequstBlock
{
    // NSLog(@"经纬度是否存在：%@",self.myDelegate.longitude);
    if (self.location.longitude.length > 0) {
        [self getUserLocationFromGpsOnFinish:aRequstBlock];
    } else {
        [self getUserLocationFromIpOnFinish:aRequstBlock];
    }
}

// Get Province City By Longtitude
- (void)getUserLocationFromGpsOnFinish:(blockVoidVoid)aFinishBlock
{
    NSString *url = [NSString stringWithFormat:@"http://api.map.baidu.com/geocoder/v2/?ak=zCHtfQYB4GIGOl1QeiOdQgkc&output=json&pois=1&location=%@,%@",
                     self.location.latitude, self.location.longitude];

    [[AFHTTPRequestOperationManager manager] GET:url
                                      parameters:nil
                                         success:^(AFHTTPRequestOperation *operation, id responseObject) {
        NSString *city = responseObject[@"result"][@"addressComponent"][@"city"];
        NSString *province = responseObject[@"result"][@"addressComponent"][@"province"];

        self.location.city = city;
        self.location.province = province;

        if (aFinishBlock) {
            aFinishBlock();
        }
    } failure:^(AFHTTPRequestOperation *operation, NSError *error) {
        NSLog(@"gps err:%@",error.localizedDescription);
    }];
}

// Get Province City By Ip
- (void)getUserLocationFromIpOnFinish:(blockVoidVoid)aFinishBlock
{
    [[AFHTTPRequestOperationManager manager] GET:@"http://api.map.baidu.com/location/ip?ak=zCHtfQYB4GIGOl1QeiOdQgkc&coor=bd09ll"
                                      parameters:nil
                                         success:^(AFHTTPRequestOperation *operation, id responseObject) {
                                            if (aFinishBlock) {
                                                aFinishBlock();
                                            }

                                            self.location.province = responseObject[@"content"][@"address_detail"][@"province"];
                                            self.location.city = responseObject[@"content"][@"address_detail"][@"city"];
                                         } failure:^(AFHTTPRequestOperation *operation, NSError *error) {
                                         }];
}

@end
