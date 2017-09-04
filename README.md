## ShadowsocksR for Android

A [shadowsocksR](https://github.com/breakwa11/shadowsocks-rss/) client for Android, written in Scala.

<a href="https://play.google.com/store/apps/details?id=com.github.shadowsocks"><img src="https://play.google.com/intl/en_us/badges/images/generic/en-play-badge.png" height="48"></a>

### CI STATUS

[![Build Status](https://api.travis-ci.org/shadowsocks/shadowsocks-android.svg)](https://travis-ci.org/shadowsocks/shadowsocks-android)

### PREREQUISITES

* JDK 1.8
* SBT 0.13.0+
* Android SDK
  - Build Tools 25+
  - Android Support Repository and Google Repository (see `build.sbt` for version)
* Android NDK r12b `High version may case something build fail`

### BUILD

*Warnning: Cannot build in windows*

* Set environment variable `ANDROID_HOME` to `/path/to/android-sdk`
* Set environment variable `ANDROID_NDK_HOME` to `/path/to/android-ndk`
* And you can set http.proxy for sbt
* Create your key following the instructions at https://developer.android.com/studio/publish/app-signing.html
* Put your key in ~/.keystore or any other place
* Create `local.properties` from `local.properties.example` with your own key information
* Invoke the building like this

```bash
    export ANDROID_HOME=/path/to/Android/Sdk/
    export ANDROID_NDK_HOME=/path/to/Android/Sdk/ndk-bundle/
    export JAVA_OPTS="$JAVA_OPTS -Dhttp.proxyHost=yourserver -Dhttp.proxyPort=port -Dhttp.proxyUser=username -Dhttp.proxyPassword=password"
```

```bash
    # install and update all git submodule
    git submodule update --init
    
    # Build the App
    sbt native-build clean android:package-release
```

##### If you use x64 linux like Archlinux x86_64, or your linux have new version ncurses lib, you may need install the 32bit version ncurses and link it as follow:

```bash
    # use Archlinux x86_64 as example
    
    # install ncurses x64 and x86 version
    sudo pacman -S lib32-ncurses ncurses
    
    # link the version-6 ncurses to version-5
    sudo ln -s /usr/lib/libncursesw.so /usr/lib/libncurses.so.5
    sudo ln -s /usr/lib32/libncursesw.so /usr/lib32/libncurses.so.5
    
    # link libncurses to libtinfo
    sudo ln -s /usr/lib/libncurses.so.5 /usr/lib/libtinfo.so.5
    sudo ln -s /usr/lib32/libncurses.so.5 /usr/lib32/libtinfo.so.5
```


#### BUILD on Mac OS X (with HomeBrew)

* Install Android SDK and NDK by run `brew install android-ndk android-sdk`
* Add `export ANDROID_HOME=/usr/local/Cellar/android-sdk/$version` to your .bashrc , then reopen the shell to load it.
* Add `export ANDROID_NDK_HOME=/usr/local/Cellar/android-ndk/$version` to your .bashrc , then reopen the shell to load it.
* echo "y" | android update sdk --filter tools,platform-tools,build-tools-23.0.2,android-23,extra-google-m2repository --no-ui -a
* echo "y" | android update sdk --filter extra-android-m2repository --no-ui --no-https -a
* Create your key following the instructions at http://developer.android.com/guide/publishing/app-signing.html#cert
* Put your key in ~/.keystore
* Create `local.properties` from `local.properties.example` with your own key information .
* Invoke the building like this

```bash
    git submodule update --init

    # Build native binaries
    ./build.sh

    # Build the apk
    sbt clean android:package-release
```

## OPEN SOURCE LICENSES

* shadowsocks-libev: [GPLv3](https://github.com/shadowsocks/shadowsocks-libev/blob/master/LICENSE)
* tun2socks: [BSD](https://github.com/shadowsocks/badvpn/blob/shadowsocks-android/COPYING)
* redsocks: [APL 2.0](https://github.com/shadowsocks/redsocks/blob/master/README)
* OpenSSL: [OpenSSL](https://github.com/shadowsocks/openssl-android/blob/master/NOTICE)
* pdnsd: [GPLv3](https://github.com/shadowsocks/shadowsocks-android/blob/master/src/main/jni/pdnsd/COPYING)
* libev: [GPLv2](https://github.com/shadowsocks/shadowsocks-android/blob/master/src/main/jni/libev/LICENSE)
* libevent: [BSD](https://github.com/shadowsocks/libevent/blob/master/LICENSE)

### LICENSE

Copyright (C) 2016 by Max Lv <<max.c.lv@gmail.com>> <br/>
Copyright (C) 2016 by Mygod Studio <<mygodstudio@gmail.com>>

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
