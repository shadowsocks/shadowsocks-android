#!/bin/bash
cd src/main
ndk-build clean
ndk-build
mkdir -p assets/armeabi
mkdir -p assets/armeabi-v7a
mkdir -p assets/x86
for app in iptables pdnsd redsocks shadowsocks tun2socks
do
    mv libs/armeabi/$app assets/armeabi/
    mv libs/armeabi-v7a/$app assets/armeabi-v7a/
    mv libs/x86/$app assets/x86/
done
