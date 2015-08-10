//
//  ADPushManager.h
//  
//
//  Created by D on 15/8/10.
//
//

#import <UIKit/UIKit.h>

@interface ADPushManager : NSObject <UIAlertViewDelegate>

@property (nonatomic, copy) NSDictionary *notifyInfo;

+ (instancetype)shareManager;

- (void)regPush;
- (void)handlePushMessageWithOptions:(NSDictionary *)launchOptions;
- (void)handlePushMessageWithApplication:(UIApplication*)application notification:(NSDictionary*)userInfo;

@end
