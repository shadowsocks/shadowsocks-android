## Shadowsocks for Android

A [shadowsocks](http://shadowsocks.org) client for Android, written in Scala.

[![Google Play](http://www.android.com/images/brand/get_it_on_play_logo_large.png)](https://play.google.com/store/apps/details?id=com.github.shadowsocks)

## TRAVIS CI STATUS

[![Build Status](https://secure.travis-ci.org/shadowsocks/shadowsocks-android.png)](http://travis-ci.org/shadowsocks/shadowsocks-android)

[Nightly Builds](http://buildbot.sinaapp.com)

## PREREQUISITES

* JDK 1.6+
* SBT 0.12.3+
* Android SDK r21+
* Android NDK r8d+

## BUILD

* Create your key following the instructions at http://developer.android.com/guide/publishing/app-signing.html#cert
* Put your key in ~/.keystore
* Create `local.sbt` from `local.sbt.example` with your own key alias
* Invoke the building like this

```bash
    # Optional
    ./build-ndk.sh

    # Build
    sbt android:prepare-market
```

## LICENSE

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
