## ShadowsocksR Android 客户端

由 Shadowsocks Android 客户端修改而来，支持 SSR 协议。

[常见问题](FAQ.md)

### 编译

准备工作

* JDK 1.8
* Android SDK
  - Android NDK

```bash
git clone --recurse-submodules https://github.com/shadowsocksRb/shadowsocksRb-android.git
cd shadowsocksRb-android
# 建议打开 mobile/build.gradle ,修改 applicationId 以规避检测
./gradlew aR
# adb install mobile/build/outputs/apk/release/mobile-release.apk
```

### 贡献

欢迎问题修复，功能添加及翻译。其中问题修复和功能添加请优先考虑为上游贡献，翻译请以简体中文为底本。

### 许可

GPLv3

使用的库

<ul>
    <li>redsocks: <a href="https://github.com/shadowsocks/redsocks/blob/shadowsocks-android/README">APL 2.0</a></li>
    <li>mbed TLS: <a href="https://github.com/ARMmbed/mbedtls/blob/development/LICENSE">APL 2.0</a></li>
    <li>libevent: <a href="https://github.com/shadowsocks/libevent/blob/master/LICENSE">BSD</a></li>
    <li>tun2socks: <a href="https://github.com/shadowsocks/badvpn/blob/shadowsocks-android/COPYING">BSD</a></li>
    <li>pcre: <a href="https://android.googlesource.com/platform/external/pcre/+/master/dist2/LICENCE">BSD</a></li>
    <li>libancillary: <a href="https://github.com/shadowsocks/libancillary/blob/shadowsocks-android/COPYING">BSD</a></li>
    <li>shadowsocksr-libev: <a href="https://github.com/shadowsocksRb/shadowsocksr-libev/blob/master/LICENSE">GPLv3</a></li>
    <li>libsodium: <a href="https://github.com/jedisct1/libsodium/blob/master/LICENSE">ISC</a></li>
</ul>

<ul>
    <li>AndroidX: <a href="https://android.googlesource.com/platform/frameworks/support/+/androidx-master-dev/LICENSE.txt">APL 2.0</a></li>
    <li>kotlin: <a href="https://github.com/JetBrains/kotlin/blob/master/license/LICENSE.txt">APL 2.0</a></li>
    <li>material: <a href="https://github.com/material-components/material-components-android/blob/master/LICENSE">APL 2.0</a></li>
    <li>gson: <a href="https://github.com/google/gson/blob/master/LICENSE">APL 2.0</a></li>
    <li>dnsjava: <a href="https://github.com/dnsjava/dnsjava/blob/master/LICENSE">BSD</a></li>
    <li>jsocks: <a href="https://android.googlesource.com/platform/external/pcre/+/master/dist2/LICENCE">LGPL 2.1</a></li>
    <li>preferencex-simplemenu: <a href="https://github.com/takisoft/preferencex-android/blob/master/LICENSE">APL 2.0</a></li>
    <li>android-plugin-api-for-locale: <a href="https://github.com/twofortyfouram/android-plugin-api-for-locale/blob/master/LICENSE.txt">APL 2.0</a></li>
    <li>qrgen: <a href="https://github.com/kenglxn/QRGen">APL 2.0</a></li>
</ul>