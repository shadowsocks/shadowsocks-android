#!/bin/bash

function try () {
"$@" || exit -1
}

try pushd src/main

# Build
#try $ANDROID_NDK_HOME/ndk-build clean
try ${ANDROID_NDK_HOME}/ndk-build -j8

# copy executables
rm -rf assets/armeabi-v7a
rm -rf assets/x86
mkdir -p assets/armeabi-v7a
mkdir -p assets/x86
for app in pdnsd redsocks ss-local ss-tunnel tun2socks
do
    try mv libs/armeabi-v7a/$app assets/armeabi-v7a/
    try mv libs/x86/$app assets/x86/
done

try popd
