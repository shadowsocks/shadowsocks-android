#!/bin/bash

ndk-build clean
ndk-build
rm -rf src/main/assets/armeabi-v7a
rm -rf src/main/assets/x86
mkdir -p src/main/assets/armeabi-v7a
mkdir -p src/main/assets/x86
for app in pdnsd redsocks
do
    mv libs/armeabi-v7a/$app src/main/assets/armeabi-v7a/
    mv libs/x86/$app src/main/assets/x86/
done

