#!/bin/bash

function try () {
"$@" || exit -1
}

# Build native binaries
pushd src/main
try $ANDROID_NDK_HOME/ndk-build -j8

rm -rf assets/armeabi-v7a
rm -rf assets/x86
mkdir -p assets/armeabi-v7a
mkdir -p assets/x86

#copy executables
for app in pdnsd redsocks ss-local ss-tunnel tun2socks
do
    echo $app
    try mv libs/armeabi-v7a/$app assets/armeabi-v7a/
    try mv libs/x86/$app assets/x86/
done
popd

# Build kcptun
pushd kcptun
try ./make.bash
popd
