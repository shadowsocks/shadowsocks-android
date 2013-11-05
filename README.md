## Shadowsocks for Android

A [shadowsocks](http://shadowsocks.org) client for Android, written in Scala.

Help to translate shadowsocks: http://crowdin.net/project/shadowsocks/invite

[![Google Play](http://developer.android.com/images/brand/en_generic_rgb_wo_45.png)](https://play.google.com/store/apps/details?id=com.github.shadowsocks)

### TRAVIS CI STATUS

[![Build Status](https://secure.travis-ci.org/shadowsocks/shadowsocks-android.png)](http://travis-ci.org/shadowsocks/shadowsocks-android)

[Nightly Builds](http://buildbot.sinaapp.com)

### PREREQUISITES

* JDK 1.6+
* SBT 0.12.4
* Android SDK r21+ ( with SDK Platform Android 4.1.2, API 16, revision 4 )
* Android NDK r9+

### BUILD

* Set environment variable `ANDROID_HOME` to `/path/to/android-sdk`
* Set environment variable `ANDROID_NDK_HOME` to `/path/to/android-ndk`
* Create your key following the instructions at http://developer.android.com/guide/publishing/app-signing.html#cert
* Put your key in ~/.keystore
* Create `local.sbt` from `local.sbt.example` with your own key alias
* Invoke the building like this

```bash
    # Build native binaries
    ./build-ndk.sh

    # Build the App
    sbt clean release
```

#### BUILD ON Mac OS X (with HomeBrew)

* Install Android SDK and NDK by run `brew install android-ndk android-sdk`
* Add `export ANDROID_HOME=/usr/local/opt/android-sdk` to your .bashrc , then reopen the shell to loat it.
* Run `android update sdk --filter tools,platform-tools,android-16 --no-ui --no-https -a` to insall SDK Platform Android 4.1.2, API 16, revision 4.
* Create your key following the instructions at http://developer.android.com/guide/publishing/app-signing.html#cert
* Put your key in ~/.keystore
* Create `local.sbt` from `local.sbt.example` with your own key alias .
* Invoke the building like this

```bash
    # Build native binaries
    ./build-ndk.sh
    
    # Build the apk
    sbt clean
    sbt apk
```

### LICENSE

Copyright (C) 2013 Max Lv <max.c.lv@gmail.com>

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
