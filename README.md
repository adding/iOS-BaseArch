# iOS-BaseArch

iOS开发基础框架 及 推荐实践，目的在于帮助开发者更快的开发iOS程序。

使用了MVVM架构，以及面向切面编程，最低版本 iOS 7.0，主要使用Objective C语言。

#### 第三方类库:
  AFNetworking
  Realm
  MOAspects
  JSONModel
  AsyncDisplayKit

  使用cocoapod 管理第三方类库。

#### 项目架构：
##### 图片组织：
  图片位于Images.xcassets中
  - Login （有关登录的图片）
  - Navigation （导航栏图片）
	- Center （titleView等位于中间的）
	- LeftItem （左侧）
	- RightItem （右侧）
  - PlaceHolder (占位符图片)
	  - Empty （空的图片）
	  - Loading （加载中的图片）
  - Share （有关分享的图片）
##### 代码组织：
- Ad Statistics （有关广告统计代码）
- Network （网络相关代码）
  - View （自定义View）
- Model (自定义模型)
- ViewModel (数据模型处理和获取)
- Manager (管理者)
	- LocationManager (位置管理者)
	- PushManager (消息推送管理者)
- Config (配置文件)
- Category (分类)
	- Color
	- Font
	- NSUserDefaults （存储应用配置信息）
	- Timer
	- Date
	- Image
	- Error （返回自定义NSError）
	- String
- Extension (扩展)
- AppDelegate
- Appearance （修改应用UI表现） 
- ViewController
- ThirdParty 
- Resource
	- Images.xcassets
	- LaunchScreen
	- Font
	- Plist
- Supporting Files
#### 项目说明：
#### 自定义分类：
#### Tips：
参考文章：

[http://casatwy.com/iosying-yong-jia-gou-tan-viewceng-de-zu-zhi-he-diao-yong-fang-an.html][1]

[1]:	http://casatwy.com/iosying-yong-jia-gou-tan-viewceng-de-zu-zhi-he-diao-yong-fang-an.html