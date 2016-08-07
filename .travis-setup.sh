#!/bin/bash

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

if [ ! -d "$GOROOT_BOOTSTRAP" ]; then
    mkdir -p $GOROOT_BOOTSTRAP
    pushd $HOME/.android
    wget https://storage.googleapis.com/golang/go1.6.3.linux-amd64.tar.gz
    tar xf go1.6.3.linux-amd64.tar.gz
    popd
fi

( sleep 5 && while [ 1 ]; do sleep 1; echo y; done ) | android update sdk --filter tools,platform-tools,build-tools-24.0.0,android-24,extra-google-m2repository --no-ui -a
( sleep 5 && while [ 1 ]; do sleep 1; echo y; done ) | android update sdk --filter extra-android-m2repository --no-ui -a
cp local.properties.travis local.properties
