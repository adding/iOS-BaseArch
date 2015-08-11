//
//  AppDelegate.m
//  iOS-BaseArch
//
//  Created by D on 15/8/10.
//  Copyright (c) 2015年 addinghome. All rights reserved.
//

#import "AppDelegate.h"
#import "ADPushManager.h"
#import "ADDataBaseManager.h"
#import "ViewController.h"

@interface AppDelegate ()

@end

@implementation AppDelegate

#pragma mark - ThirdParty
- (void)initThirdParty {
    
}

#pragma mark - set Vc
- (void)setRootVc {
    self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    [self.window makeKeyAndVisible];
    self.window.backgroundColor = [UIColor whiteColor];
    
    self.window.rootViewController = [[ViewController alloc]init];
}

#pragma mark - Application Circle Life
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    [[ADPushManager shareManager] regPush];
    [[ADPushManager shareManager] handlePushMessageWithOptions:launchOptions];
    [[ADDataBaseManager shareManager] migrationDataBase];
    
    [self initThirdParty];
    
    [self setRootVc];
    
    return YES;
}

- (void)applicationWillResignActive:(UIApplication *)application {
}

- (void)applicationDidEnterBackground:(UIApplication *)application {
}

- (void)applicationWillEnterForeground:(UIApplication *)application {
}

- (void)applicationDidBecomeActive:(UIApplication *)application {
}

- (void)applicationWillTerminate:(UIApplication *)application {

}

- (void)application:(UIApplication*)application didReceiveRemoteNotification:(NSDictionary*)userInfo {
    [[ADPushManager shareManager] handlePushMessageWithApplication:application notification:userInfo];
}

@end
