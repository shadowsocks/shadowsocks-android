#!/bin/bash
cd src/main
ndk-build
for app in ip6tables iptables obfsproxy pdnsd polipo redsocks shadowsocks tun2socks
do
    mv libs/armeabi/$app assets/arm/
    mv libs/x86/$app assets/x86/
done
