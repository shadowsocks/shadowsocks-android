#!/bin/bash

export ARCH=`uname -m`
export GOROOT_BOOTSTRAP=.android/go
export ANDROID_NDK_HOME=$HOME/.android/android-ndk-r12b
export ANDROID_HOME=$HOME/.android/android-sdk-linux
export PATH=${ANDROID_NDK_HOME}:${ANDROID_HOME}/tools:${ANDROID_HOME}/platform-tools:${PATH}

if [ ! -d "$GOROOT_BOOTSTRAP" ]; then
    wget https://storage.googleapis.com/golang/go1.6.3.linux-amd64.tar.gz
    tar xvf go1.6.3.linux-amd64.tar.gz
    mv go .android/
fi

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
    wget -q http://dl.google.com/android/repository/android-ndk-r12b-linux-${ARCH}.zip
    unzip -q android-ndk-r12b-linux-${ARCH}.zip
    popd
fi

( sleep 5 && while [ 1 ]; do sleep 1; echo y; done ) | android update sdk --filter tools,platform-tools,build-tools-24.0.0,android-23,android-24,extra-google-m2repository --no-ui -a
( sleep 5 && while [ 1 ]; do sleep 1; echo y; done ) | android update sdk --filter extra-android-m2repository --no-ui -a
