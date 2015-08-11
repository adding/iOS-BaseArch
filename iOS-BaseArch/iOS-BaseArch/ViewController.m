//
//  ViewController.m
//  iOS-BaseArch
//
//  Created by D on 15/8/10.
//  Copyright (c) 2015年 addinghome. All rights reserved.
//

#import "ViewController.h"

@interface ViewController ()

@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];

    [self gcdGroup];
}

// 等待两个任务执行后 再执行
- (void)gcdGroup
{
    __block UIImage *image;
    __block NSString *content;
    
    void (^loadImageFromInternet)(void) = ^(void) {
        // code for load image from internet, slowly.....
        //....
        sleep(2);
        image = [UIImage imageNamed:@""];
    };
    
    void (^loadContentFromInternet)(void) = ^(void) {
        // code for load content from internet, slowly.....
        //....
        sleep(3);
        content = @"adsf";
    };
    
    // create group
    dispatch_group_t group = dispatch_group_create();
    dispatch_group_async(group, dispatch_get_global_queue(0, 0), loadImageFromInternet);
    dispatch_group_async(group, dispatch_get_global_queue(0, 0), loadContentFromInternet);
    
    // process result
    dispatch_group_notify(group, dispatch_get_global_queue(0, 0), ^(void) {
        NSLog(@"%@, %@", image, content);
    });
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

@end
