## Shadowsocks for Android

A [shadowsocks](http://shadowsocks.org) client for Android, written in Scala.

<a href="https://play.google.com/store/apps/details?id=com.github.shadowsocks"><img src="https://play.google.com/intl/en_us/badges/images/generic/en-play-badge.png" height="48"></a>

### CI STATUS

[![Build Status](https://api.travis-ci.org/shadowsocks/shadowsocks-android.svg)](https://travis-ci.org/shadowsocks/shadowsocks-android)

### PREREQUISITES

* JDK 1.8
* SBT 0.13.0+
* Go 1.4+
* Android SDK
  - Build Tools 25+
  - Android Support Repository and Google Repository (see `build.sbt` for version)
* Android NDK r12b+

### BUILD

* Set environment variable `ANDROID_HOME` to `/path/to/android-sdk`
* Set environment variable `ANDROID_NDK_HOME` to `/path/to/android-ndk`
* Set environment variable `GOROOT_BOOTSTRAP` to `/path/to/go`
* Create your key following the instructions at https://developer.android.com/studio/publish/app-signing.html
* Create `local.properties` from `local.properties.example` with your own key information
* Invoke the building like this

```bash
    git submodule update --init

    # Build the App
    sbt native-build clean android:package-release
```

## OPEN SOURCE LICENSES

<ul>
    <li>redsocks: <a href="https://github.com/shadowsocks/redsocks/blob/shadowsocks-android/README">APL 2.0</a></li>
    <li>mbed TLS: <a href="https://github.com/ARMmbed/mbedtls/blob/development/LICENSE">APL 2.0</a></li>
    <li>libevent: <a href="https://github.com/shadowsocks/shadowsocks-android/blob/master/src/main/jni/libevent/LICENSE">BSD</a></li>
    <li>tun2socks: <a href="https://github.com/shadowsocks/badvpn/blob/shadowsocks-android/COPYING">BSD</a></li>
    <li>pcre: <a href="https://android.googlesource.com/platform/external/pcre/+/master/dist2/LICENCE">BSD</a></li>
    <li>libancillary: <a href="https://github.com/shadowsocks/libancillary/blob/shadowsocks-android/COPYING">BSD</a></li>
    <li>shadowsocks-libev: <a href="https://github.com/shadowsocks/shadowsocks-libev/blob/master/LICENSE">GPLv3</a></li>
    <li>pdnsd: <a href="https://github.com/shadowsocks/shadowsocks-android/blob/master/src/main/jni/pdnsd/COPYING">GPLv3</a></li>
    <li>libev: <a href="https://github.com/shadowsocks/shadowsocks-libev/blob/master/libev/LICENSE">GPLv2</a></li>
    <li>kcptun: <a href="https://github.com/xtaci/kcptun/commits/master/LICENSE.md">MIT</a></li>
    <li>libsodium: <a href="https://github.com/jedisct1/libsodium/blob/master/LICENSE">ISC</a></li>
</ul>

### LICENSE

Copyright (C) 2017 by Max Lv <<max.c.lv@gmail.com>>  
Copyright (C) 2017 by Mygod Studio <<contact-shadowsocks-android@mygod.be>>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
