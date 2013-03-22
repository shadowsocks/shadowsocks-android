#!/bin/bash
ndk-build
for app in ip6tables iptables obfsproxy pdnsd polipo redsocks shadowsocks
do
    cp libs/armeabi/$app assets/arm/
    cp libs/x86/$app assets/x86/
done
