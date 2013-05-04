## Shadowsocks for Android

A [shadowsocks](http://shadowsocks.org) client for Android, powered by amazing nodejs/golang/libev.

## TRAVIS CI STATUS

[![Build Status](https://secure.travis-ci.org/shadowsocks/shadowsocks-android.png)](http://travis-ci.org/shadowsocks/shadowsocks-android)

[Nightly Builds](http://buildbot.sinaapp.com)

## PREREQUISITES

* JDK 1.6+
* SBT 0.12.3+
* Android SDK r17+
* Android NDK r8d+

## BUILD

* Create your key following the instructions at http://developer.android.com/guide/publishing/app-signing.html#cert
* Put your key in ~/.keystore
* Create local.sbt from local.sbt.example with your own key alias
* Invoke the building like this

```bash
    # Optional
    ./build-ndk.sh

    # Build
    sbt android:prepare-market
```
