# iOS-BaseArch

iOS开发基础框架 及 推荐实践，目的在于帮助开发者更快的开发iOS程序。

使用了MVVM架构，以及面向切面编程，最低版本 iOS 7.0，主要使用Objective C语言。

**为什么使用 MVVM**

- MVVM 可以兼容你当下使用的 MVC 架构
- MVVM 增加你的应用的可测试性
- 简化了控制器内容
- 归并了工具类
- 易于动态映射

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
- Ad Statistics （广告统计代码）
- Network （网络相关代码）
- View （自定义View）
	- Button
	- Cell 
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

#### 项目文件说明：
##### 自定义分类：
#### 存储方案：
**为什么使用 Realm**
- 速度快
- 有开源社区支持，不断更新增加API
- Realm Browser(数据库文件浏览器，可以方便的修改数据)
- 模型 既 数据
- 数据库迁移方便
	 
**为什么使用 KeyChain**
- 只存储用户登录信息
- 存储信息不会随App删除 而丢失
- 可以通过 Group Access 共享登录信息

**关于 NSUserDefaults**
- 只存储有关应用配置方面的信息
- 存储方便
- 性能差

#### Tips：
##### ViewModel 适合做：
- 处理网络或者数据库请求
- 检查信息是否该显示
- Date 和 number 等类型的格式化
- 本地化字符

##### 为什么推荐在 MVVM 模式下 使用 ReactiveCocoa?
在 Model 改变时，我们希望能够及时的更新 ViewModel 和 View，这个时候使用ReactiveCocoa 能够 动态响应并修改我们的 ViewModel。
---- 
参考文章：

[http://casatwy.com/iosying-yong-jia-gou-tan-viewceng-de-zu-zhi-he-diao-yong-fang-an.html][1]

[1]:	http://casatwy.com/iosying-yong-jia-gou-tan-viewceng-de-zu-zhi-he-diao-yong-fang-an.html