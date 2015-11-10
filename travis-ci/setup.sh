#!/bin/bash

export ARCH=`uname -m`
export ANDROID_NDK_HOME=$HOME/.android/android-ndk-r10d
export ANDROID_HOME=$HOME/.android/android-sdk-linux
export PATH=${ANDROID_NDK_HOME}:${ANDROID_HOME}/tools:${ANDROID_HOME}/platform-tools:${PATH}

if [ ! -d "$ANDROID_HOME" ]; then
    mkdir -p $ANDROID_HOME
    pushd $HOME/.android
    wget -q http://dl.google.com/android/android-sdk_r24.4.1-linux.tgz
    tar xf android-sdk_r24.4.1-linux.tgz
    popd
fi

if [ ! -d "$ANDROID_NDK_HOME" ]; then
    mkdir -p $ANDROID_NDK_HOME
    pushd $HOME/.android
    wget -q http://dl.google.com/android/ndk/android-ndk-r10d-linux-${ARCH}.bin
    chmod +x android-ndk-r10d-linux-${ARCH}.bin
    ./android-ndk-r10d-linux-${ARCH}.bin -y &> /dev/null
    popd
fi

echo "y" | android update sdk --filter tools,platform-tools,build-tools-23.0.2,android-23,extra-google-m2repository --no-ui -a
echo "y" | android update sdk --filter extra-android-m2repository --no-ui -a
