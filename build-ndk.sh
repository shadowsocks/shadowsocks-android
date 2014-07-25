#!/bin/bash

# Target 9
cp jni/Application.mk.9 jni/Application.mk
ndk-build clean
ndk-build
rm -rf src/main/assets/armeabi
rm -rf src/main/assets/armeabi-v7a
rm -rf src/main/assets/x86
mkdir -p src/main/assets/armeabi
mkdir -p src/main/assets/armeabi-v7a
mkdir -p src/main/assets/x86
for app in pdnsd redsocks
do
    mv libs/armeabi/$app src/main/assets/armeabi/
    mv libs/armeabi-v7a/$app src/main/assets/armeabi-v7a/
    mv libs/x86/$app src/main/assets/x86/
done
rm -rf src/main/libs
mv libs src/main/

# Target 16
cp jni/Application.mk.16 jni/Application.mk
ndk-build clean
ndk-build
rm -rf src/main/assets/api-16/armeabi
rm -rf src/main/assets/api-16/armeabi-v7a
rm -rf src/main/assets/api-16/x86
mkdir -p src/main/assets/api-16/armeabi
mkdir -p src/main/assets/api-16/armeabi-v7a
mkdir -p src/main/assets/api-16/x86
for app in pdnsd redsocks
do
    mv libs/armeabi/$app src/main/assets/api-16/armeabi/
    mv libs/armeabi-v7a/$app src/main/assets/api-16/armeabi-v7a/
    mv libs/x86/$app src/main/assets/api-16/x86/
done

