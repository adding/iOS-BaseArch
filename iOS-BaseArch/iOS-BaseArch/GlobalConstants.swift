//
//  GlobalConstants.swift
//  PregnantAssistant
//
//  Created by D on 15/5/25.
//  Copyright (c) 2015年 Adding. All rights reserved.
//

import UIKit

let screenWidth = UIScreen.mainScreen().bounds.size.width
let screenHeight = UIScreen.mainScreen().bounds.size.height

//#warning test
let baseApiUrl = "http://api.addinghome.com"

struct action {
    static let content = "adding://adco/content"
    static let video = "adding://adco/video"
    static let webview = "adding://adco/webview"
    static let tool = "adding://adco/adTool"
}

let Device = UIDevice.currentDevice()

private let iosVersion = NSString(string: Device.systemVersion).doubleValue

let iOS8 = iosVersion >= 8
let iOS7 = iosVersion >= 7 && iosVersion < 8