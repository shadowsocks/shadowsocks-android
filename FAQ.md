## 1.二维码扫描功能

需要安装[条码扫描器](https://play.google.com/store/apps/details?id=com.google.zxing.client.android).
不内置扫描功能.

## 2.界面变化太大

由于Scala on Android已经停止维护多年，所以基于最新的 SS Android 客户端开始修改。

## 3.与SS客户端相比，修改了什么

砍了gms，删除日志上报，行为分析和广告。此应用是完全自由的，没有接入第三方服务，不上传任何信息。
砍了ACL编辑功能，插件功能和TV版，将服务端改成SSR 。

## 4.与原SSR客户端相比，有什么区别

砍了Nat模式，用透明代理模式替代。请root用户自行配置防火墙，将流量转发至透明代理端口。
添加了仅代理模式。适用于仅使用支持配置socks5代理的应用。
砍了前置代理。
砍了China DNS，换用现客户端的新DNS解析方式，可能与SSR服务端有兼容性问题。
修改颜色为水鸭青。

## 5.插件栏的UDP配置是什么

将UDP改为通过此服务器转发

## 6.下载地址

* [GitHub](https://github.com/shadowsocksRb/shadowsocksRb-android/releases)
* [Telegram](https://t.me/ShadowsocksRb)
