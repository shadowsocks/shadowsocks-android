#!/bin/bash

function try () {
"$@" || exit -1
}

pushd kcptun
try ./make.bash
popd

try pushd src/main

# Build
#try $ANDROID_NDK_HOME/ndk-build clean
try $ANDROID_NDK_HOME/ndk-build -j8

# copy executables
for app in pdnsd redsocks ss-local ss-tunnel tun2socks
do
    rm -f assets/armeabi-v7a/$app
    rm -f assets/x86/$app
    try mv libs/armeabi-v7a/$app assets/armeabi-v7a/
    try mv libs/x86/$app assets/x86/
done

try popd
