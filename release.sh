#!/bin/bash

release=$1
cp mobile/build/outputs/apk/release/mobile-armeabi-v7a-release.apk shadowsocks-armeabi-v7a-${release}.apk
cp mobile/build/outputs/apk/release/mobile-arm64-v8a-release.apk shadowsocks-arm64-v8a-${release}.apk
cp mobile/build/outputs/apk/release/mobile-x86-release.apk shadowsocks-x86-${release}.apk
cp mobile/build/outputs/apk/release/mobile-x86_64-release.apk shadowsocks-x86_64-${release}.apk
cp mobile/build/outputs/apk/release/mobile-universal-release.apk shadowsocks--universal-${release}.apk
cp tv/build/outputs/apk/release/tv-armeabi-v7a-release.apk shadowsocks-tv-armeabi-v7a-${release}.apk
cp tv/build/outputs/apk/release/tv-arm64-v8a-release.apk shadowsocks-tv-arm64-v8a-${release}.apk
cp tv/build/outputs/apk/release/tv-x86-release.apk shadowsocks-tv-x86-${release}.apk
cp tv/build/outputs/apk/release/tv-x86_64-release.apk shadowsocks-tv-x86_64-${release}.apk
cp tv/build/outputs/apk/release/tv-universal-release.apk shadowsocks-tv--universal-${release}.apk
