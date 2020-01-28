## 二维码扫描功能

依最小权限原则，删除非核心功能的QR码扫描。改为使用外部工具处理，需要安装[条码扫描器](https://play.google.com/store/apps/details?id=com.google.zxing.client.android)。

无法访问Google Play可以[从GitHub下载](https://github.com/shadowsocksRb/zxing-android/releases/latest)。

## 插件栏的UDP配置是什么

将UDP改为通过此服务器转发

## 修改混淆参数

来自订阅的配置文件无法编辑服务器设置部分，即使编辑，修改内容也会在更新订阅后丢失。

如果想要修改，并且远程服务器配置也允许修改，选中此配置文件后点按新建菜单的手动设置项，生成此配置文件的副本。副本脱离订阅，可以随意修改。

## 批量修改配置文件

除分应用VPN和UDP配置外，不提供批量修改功能。

但有一些小技巧：

在手动添加新配置文件时，会从当前已选中配置文件复制大部分内容。从订阅新增的配置文件则仅复制*功能设置*内容。

在进行批量添加前，先设置好已选中的配置文件会方便很多。

## 需要批量测试功能

参考[shadowsocks#issue2215](https://github.com/shadowsocks/shadowsocks-android/issues/2215)，目前本项目处于等待上游实现状态。

确认过原SSR实现，问题太多，取消移植。

## 各ROM兼容性问题

转至[shadowsocks常见问题](https://github.com/shadowsocks/shadowsocks-android/blob/master/.github/faq.md#why-is-my-rom-not-supported)查看，包括部分应用无法联网，没有流量等问题。

## 国内应用网速缓慢

使用分应用VPN功能绕过对国内应用的代理。

## 界面变化太大

Scala on Android已经停止维护多年，本项目基于最新SS Android客户端修改添加SSR特性，而非继续在原版SSR客户端上修补。

## 与SS客户端相比，修改了什么

移除gms依赖，删除日志上报，行为分析和广告。此应用没有接入第三方服务，不上传任何信息。

删除ACL编辑功能，插件功能和TV版，核心替换为SSR。

## 与原SSR客户端相比，有什么区别

删除Nat模式，用透明代理模式替代。请root用户自行配置防火墙，将流量转发至透明代理端口。

添加仅代理模式。适用于仅使用支持配置socks5代理的应用。

删除前置代理，批量测试。

删除China DNS，换用现客户端的新DNS解析方式，可能与SSR核心有兼容性问题。

修改颜色为水鸭青。

## 可以添加SSRR的新协议吗

不能。本项目不会修改SSR协议部分，仅同步SS上游和作客户端兼容性维护。

## 对界面有意见或建议

本项目同步SS上游，非SSR专有特性的意见或建议请向上游反馈。

## 下载地址

* [GitHub](https://github.com/shadowsocksRb/shadowsocksRb-android/releases)
* [Telegram](https://t.me/ShadowsocksRb)
