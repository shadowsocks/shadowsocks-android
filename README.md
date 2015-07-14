## Shadowsocks for Android

A [shadowsocks](http://shadowsocks.org) client for Android, written in Scala.

Help to translate shadowsocks: http://crowdin.net/project/shadowsocks/invite

[![Google Play](http://developer.android.com/images/brand/en_generic_rgb_wo_45.png)](https://play.google.com/store/apps/details?id=com.github.shadowsocks)

### CI STATUS

[![Build Status](https://drone.io/github.com/shadowsocks/shadowsocks-android/status.png)](https://drone.io/github.com/shadowsocks/shadowsocks-android/latest)

### PREREQUISITES

* JDK 1.7+
* SBT 0.13.0+
* Android SDK r24+
* Android NDK r10d+

### BUILD

* Set environment variable `ANDROID_HOME` to `/path/to/android-sdk`
* Set environment variable `ANDROID_NDK_HOME` to `/path/to/android-ndk`
* Create your key following the instructions at http://developer.android.com/guide/publishing/app-signing.html#cert
* Put your key in ~/.keystore
* Create `local.properties` from `local.properties.example` with your own key information
* Invoke the building like this

```bash
    git submodule update --init

    # Build native binaries
    ./build.sh
    
    # Build the App
    sbt clean android:package-release
```

#### BUILD on Mac OS X (with HomeBrew)

* Install Android SDK and NDK by run `brew install android-ndk android-sdk`
* Add `export ANDROID_HOME=/usr/local/Cellar/android-sdk/$version` to your .bashrc , then reopen the shell to load it.
* Add `export ANDROID_NDK_HOME=/usr/local/Cellar/android-ndk/$version` to your .bashrc , then reopen the shell to load it.
* echo "y" | android update sdk --filter tools,platform-tools,build-tools-21.0.1,android-21,extra-google-m2repository --no-ui --no-https -a
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
* tun2socks: [BSD](https://github.com/shadowsocks/shadowsocks-android/blob/master/src/main/jni/badvpn/COPYING)
* redsocks: [APL 2.0](https://github.com/shadowsocks/shadowsocks-android/blob/master/src/main/jni/redsocks/README)
* OpenSSL: [OpenSSL](https://github.com/shadowsocks/shadowsocks-android/blob/master/src/main/jni/openssl/NOTICE)
* pdnsd: [GPLv3](https://github.com/shadowsocks/shadowsocks-android/blob/master/src/main/jni/pdnsd/COPYING)
* libev: [GPLv2](https://github.com/shadowsocks/shadowsocks-android/blob/master/src/main/jni/libev/LICENSE)
* libevent: [BSD](https://github.com/shadowsocks/shadowsocks-android/blob/master/src/main/jni/libevent/LICENSE)

### LICENSE

Copyright (C) 2014 Max Lv <max.c.lv@gmail.com>

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
