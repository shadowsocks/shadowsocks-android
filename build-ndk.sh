#!/bin/bash
ndk-build clean
ndk-build
mkdir -p assets/armeabi
mkdir -p assets/armeabi-v7a
mkdir -p assets/x86
for app in ip6tables iptables pdnsd redsocks ss-local ss-tunnel tun2socks
do
    mv libs/armeabi/$app src/main/assets/armeabi/
    mv libs/armeabi-v7a/$app src/main/assets/armeabi-v7a/
    mv libs/x86/$app src/main/assets/x86/
done
mv libs src/main/jni
