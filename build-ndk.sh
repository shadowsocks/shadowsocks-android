#!/bin/bash
cd src/main
ndk-build clean
ndk-build
mkdir -p assets/arm
mkdir -p assets/x86
for app in iptables pdnsd redsocks shadowsocks tun2socks
do
    mv libs/armeabi/$app assets/arm/
    mv libs/x86/$app assets/x86/
done
