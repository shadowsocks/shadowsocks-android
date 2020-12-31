## [Shadowsocks](https://shadowsocks.org) for Android

[![CircleCI](https://circleci.com/gh/shadowsocks/shadowsocks-android.svg?style=shield)](https://circleci.com/gh/shadowsocks/shadowsocks-android)
[![API](https://img.shields.io/badge/API-23%2B-brightgreen.svg?style=flat)](https://android-arsenal.com/api?level=23)
[![Releases](https://img.shields.io/github/downloads/shadowsocks/shadowsocks-android/total.svg)](https://github.com/shadowsocks/shadowsocks-android/releases)
[![Language: Kotlin](https://img.shields.io/github/languages/top/shadowsocks/shadowsocks-android.svg)](https://github.com/shadowsocks/shadowsocks-android/search?l=kotlin)
[![Codacy Badge](https://api.codacy.com/project/badge/Grade/1a21d48d466644cdbcb57a1889abea5b)](https://www.codacy.com/app/shadowsocks/shadowsocks-android?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=shadowsocks/shadowsocks-android&amp;utm_campaign=Badge_Grade)
[![License: GPL-3.0](https://img.shields.io/badge/license-GPL--3.0-orange.svg)](https://www.gnu.org/licenses/gpl-3.0)

<a href="https://play.google.com/store/apps/details?id=com.github.shadowsocks"><img src="https://play.google.com/intl/en_us/badges/images/generic/en-play-badge.png" height="48"></a>
for Android & Chrome OS ([beta](https://play.google.com/apps/testing/com.github.shadowsocks))  
<a href="https://play.google.com/store/apps/details?id=com.github.shadowsocks.tv"><img src="https://play.google.com/intl/en_us/badges/images/generic/en-play-badge.png" height="48"></a>
for Android TV ([beta](https://play.google.com/apps/testing/com.github.shadowsocks.tv))


### PREREQUISITES

* JDK 1.8
* Android SDK
  - Android NDK
* Rust with Android targets installed  

```bash
  $ cd core/src/main/rust/shadowsocks-rust
  $ rustup target add armv7-linux-androideabi aarch64-linux-android i686-linux-android x86_64-linux-android
```

### BUILD

You can check whether the latest commit builds under UNIX environment by checking Travis status.

* Install prerequisites
* Clone the repo using `git clone --recurse-submodules <repo>` or update submodules using `git submodule update --init --recursive`
* Build it using Android Studio or gradle script

### BUILD WITH DOCKER

* Clone the repo using `git clone --recurse-submodules <repo>` or update submodules using `git submodule update --init --recursive`
* Run `docker run --rm -v ${PWD}:/build -w /build shadowsocks/android-ndk-go ./gradlew assembleDebug`

### CONTRIBUTING

If you are interested in contributing or getting involved with this project, please read the CONTRIBUTING page for more information.  The page can be found [here](https://github.com/shadowsocks/shadowsocks-android/blob/master/CONTRIBUTING.md).


### [TRANSLATE](https://discourse.shadowsocks.org/t/poeditor-translation-main-thread/30)

## OPEN SOURCE LICENSES

<ul>
    <li>redsocks: <a href="https://github.com/shadowsocks/redsocks/blob/shadowsocks-android/README">APL 2.0</a></li>
    <li>libevent: <a href="https://github.com/shadowsocks/libevent/blob/master/LICENSE">BSD</a></li>
    <li>tun2socks: <a href="https://github.com/shadowsocks/badvpn/blob/shadowsocks-android/COPYING">BSD</a></li>
    <li>shadowsocks-rust: <a href="https://github.com/shadowsocks/shadowsocks-rust/blob/master/LICENSE">MIT</a></li>
    <li>libsodium: <a href="https://github.com/jedisct1/libsodium/blob/master/LICENSE">ISC</a></li>
    <li>OpenSSL: <a href="https://www.openssl.org/source/license-openssl-ssleay.txt">OpenSSL License</a></li>
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
