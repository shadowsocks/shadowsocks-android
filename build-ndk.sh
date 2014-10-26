#!/bin/bash

function try () {
    "$@" || exit -1
}

# Build
try $ANDROID_NDK_HOME/ndk-build clean
try $ANDROID_NDK_HOME/ndk-build

# copy executables
rm -rf src/main/assets/armeabi-v7a
rm -rf src/main/assets/x86
mkdir -p src/main/assets/armeabi-v7a
mkdir -p src/main/assets/x86
for app in pdnsd redsocks iptables
do
    try mv libs/armeabi-v7a/$app src/main/assets/armeabi-v7a/
    try mv libs/x86/$app src/main/assets/x86/
done

# copy libraries
rm -rf src/main/jni/armeabi-v7a
rm -rf src/main/jni/x86
mkdir -p src/main/jni/armeabi-v7a
mkdir -p src/main/jni/x86
try mv libs/armeabi-v7a/*.so src/main/jni/armeabi-v7a/
try mv libs/x86/*.so src/main/jni/x86/
