//
//  ADPushManager.m
//  
//
//  Created by D on 15/8/10.
//
//

#import "ADPushManager.h"

@implementation ADPushManager

+ (ADPushManager *)shareManager {
    static dispatch_once_t onceToken;
    static ADPushManager *sharedPushManager = nil;
    
    dispatch_once(&onceToken, ^{
        sharedPushManager = [[ADPushManager alloc] init];
    });
    
    return sharedPushManager;
}

- (void)regPush
{
    if(IOS8_OR_LATER) {
        [[ADPushManager shareManager] registerPushForIOS8];
    } else {
        [[ADPushManager shareManager] registerPush];
    }
}

- (void)registerPush {
    [[UIApplication sharedApplication] registerForRemoteNotificationTypes:(UIRemoteNotificationTypeAlert | UIRemoteNotificationTypeBadge | UIRemoteNotificationTypeSound)];
}

- (void)registerPushForIOS8 {
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= _IPHONE80_
    
    //Types
    UIUserNotificationType types = UIUserNotificationTypeBadge | UIUserNotificationTypeSound | UIUserNotificationTypeAlert;
    
    //Actions
    UIMutableUserNotificationAction *acceptAction = [[UIMutableUserNotificationAction alloc] init];
    
    acceptAction.identifier = @"ACCEPT_IDENTIFIER";
    acceptAction.title = @"Accept";
    
    acceptAction.activationMode = UIUserNotificationActivationModeForeground;
    acceptAction.destructive = NO;
    acceptAction.authenticationRequired = NO;
    
    //Categories
    UIMutableUserNotificationCategory *inviteCategory = [[UIMutableUserNotificationCategory alloc] init];
    
    inviteCategory.identifier = @"INVITE_CATEGORY";
    
    [inviteCategory setActions:@[acceptAction] forContext:UIUserNotificationActionContextDefault];
    
    [inviteCategory setActions:@[acceptAction] forContext:UIUserNotificationActionContextMinimal];
    
    NSSet *categories = [NSSet setWithObjects:inviteCategory, nil];
    
    UIUserNotificationSettings *mySettings = [UIUserNotificationSettings settingsForTypes:types categories:categories];
    
    [[UIApplication sharedApplication] registerUserNotificationSettings:mySettings];
    
    [[UIApplication sharedApplication] registerForRemoteNotifications];
#endif
}

#pragma mark - handle msg method
- (void)handlePushMessageWithOptions:(NSDictionary *)launchOptions {
    if (!launchOptions) {
        return;
    }
    
    NSDictionary* pushNotification= launchOptions[UIApplicationLaunchOptionsRemoteNotificationKey];
//    NSLog(@"获得的推送消息：%@",pushNotificationKey);
    
    if (pushNotification) {
        self.notifyInfo = pushNotification;
        [self analysisRemoteNotification];
    }
}

- (void)handlePushMessageWithApplication:(UIApplication*)application notification:(NSDictionary*)userInfo {
    self.notifyInfo = userInfo;
    if (application.applicationState == UIApplicationStateActive) {
        // 在前台 可以 弹出 AlertView
        UIAlertView  *alertView = [[UIAlertView alloc] initWithTitle:@"来通知啦"
                                                             message:@"看看吗"
                                                            delegate:self
                                                   cancelButtonTitle:@"关闭"
                                                   otherButtonTitles:@"查看", nil];
        [alertView show];
    } else {
        [self analysisRemoteNotification];
    }
}

- (void)analysisRemoteNotification
{
    
}

#pragma mark - AlertView Method
- (void)alertView:(UIAlertView *)alertView clickedButtonAtIndex:(NSInteger)buttonIndex
{
    if (buttonIndex == 1) {
        [self analysisRemoteNotification];
    }
}

@end
