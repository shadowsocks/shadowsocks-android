## 1.二维码扫描功能

需要安装[条码扫描器](https://play.google.com/store/apps/details?id=com.google.zxing.client.android).
不内置扫描功能.

## 2.界面变化太大

由于Scala on Android已经停止维护多年，所以基于最新的 SS Android 客户端开始修改。

## 3.与SS客户端相比，修改了什么

砍了gms，删除日志上报，行为分析和广告。此应用是完全自由的，没有接入第三方服务，不上传任何信息。

砍了ACL编辑功能，插件功能和TV版，将服务端改成SSR。

## 4.与原SSR客户端相比，有什么区别

砍了Nat模式，用透明代理模式替代。请root用户自行配置防火墙，将流量转发至透明代理端口。

添加了仅代理模式。适用于仅使用支持配置socks5代理的应用。

砍了前置代理，批量测试。

砍了China DNS，换用现客户端的新DNS解析方式，可能与SSR服务端有兼容性问题。

修改颜色为水鸭青。

## 5.插件栏的UDP配置是什么

将UDP改为通过此服务器转发

## 6.下载地址

* [GitHub](https://github.com/shadowsocksRb/shadowsocksRb-android/releases)
* [Telegram](https://t.me/ShadowsocksRb)

## 7.输入订阅链接无反应

通过明文传递信息是不安全的，如果您的订阅链接是HTTP链接，它将会被静默的忽略。请要求服务提供商将订阅链接升级到HTTPS以避免节点信息泄露。

## 8.想要批量修改配置文件

除分应用VPN外，不提供批量修改功能。

但有一些小技巧：

在手动添加新配置文件时，会从当前已选中配置文件复制大部分内容。从订阅新增的配置文件则仅复制*功能设置*内容。

在进行批量添加前，先设置好已选中的配置文件会方便很多。

## 9.需要批量测试功能

参考[shadowsocks#issue2215](https://github.com/shadowsocks/shadowsocks-android/issues/2215)，目前本项目处于等待上游实现状态。

确认过原SSR实现，问题太多，取消移植。

## 10.项目开发方向

保持与SS Android客户端上游同步，不动SSR协议部分。本项目不会重启SSR开发，仅仅出于原SSR客户端因Android版本升级出现问题，而开始进行必要的维护。

## 11.会维护其它客户端吗

其它平台相对稳定，不会因程序本身而出现兼容性问题。

但欢迎开发者加入，维护其它平台。~~本项目的近期目标是将SSR插件移植到最新shadowsocks-libev。~~

## 12.可以添加SSRR的新协议吗

不能。shadowsocksRb永远不会触碰SSR协议部分，仅同步SS上游和作客户端兼容性维护。

## 13.各ROM兼容性问题

转至[shadowsocks 常见问题](https://github.com/shadowsocks/shadowsocks-android/blob/master/.github/faq.md#why-is-my-rom-not-supported)查看，包括部分应用无法联网，没有流量等问题。
