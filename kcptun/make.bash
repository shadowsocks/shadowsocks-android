#!/bin/bash

function try () {
"$@" || exit -1
}

[ -z "$ANDROID_NDK_HOME" ] && ANDROID_NDK_HOME=~/android-ndk-r12b

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
DEPS=$DIR/.deps
ANDROID_ARM_TOOLCHAIN=$DEPS/android-toolchain-16-arm
ANDROID_X86_TOOLCHAIN=$DEPS/android-toolchain-16-x86

ANDROID_ARM_CC=$ANDROID_ARM_TOOLCHAIN/bin/arm-linux-androideabi-gcc
ANDROID_ARM_STRIP=$ANDROID_ARM_TOOLCHAIN/bin/arm-linux-androideabi-strip

ANDROID_X86_CC=$ANDROID_X86_TOOLCHAIN/bin/i686-linux-android-gcc
ANDROID_X86_STRIP=$ANDROID_X86_TOOLCHAIN/bin/i686-linux-android-strip


if [ ! -d "$DEPS" ]; then
    mkdir -p $DEPS 
fi

if [ ! -d "$ANDROID_ARM_TOOLCHAIN" ]; then
    echo "Make standalone toolchain for ARM arch"
    $ANDROID_NDK_HOME/build/tools/make_standalone_toolchain.py --arch arm \
        --api 16 --install-dir $ANDROID_ARM_TOOLCHAIN
fi

if [ ! -d "$ANDROID_X86_TOOLCHAIN" ]; then
    echo "Make standalone toolchain for X86 arch"
    $ANDROID_NDK_HOME/build/tools/make_standalone_toolchain.py --arch x86 \
        --api 16 --install-dir $ANDROID_X86_TOOLCHAIN
fi

if [ ! -d "$DIR/go/bin" ]; then
    echo "Build the custom go"

    pushd $DIR/go/src
    try ./make.bash
    popd
fi

export GOROOT=$DIR/go
export GOPATH=$DEPS/gopath
export GOBIN=$GOPATH/bin
mkdir -p $GOBIN
export PATH=$GOROOT/bin:$PATH

pushd kcptun/client

echo "Get dependences for kcptun"
go get -u github.com/xtaci/kcp-go
go get

echo "Cross compile kcptun for arm"
try env CGO_ENABLED=1 CC=$ANDROID_ARM_CC GOOS=android GOARCH=arm GOARM=7 go build -ldflags="-s -w"
try $ANDROID_ARM_STRIP client
try mv client $DIR/../src/main/assets/armeabi-v7a/kcptun

echo "Cross compile kcptun for x86"
try env CGO_ENABLED=1 CC=$ANDROID_X86_CC GOOS=android GOARCH=386 go build -ldflags="-s -w"
try $ANDROID_X86_STRIP client
try mv client $DIR/../src/main/assets/x86/kcptun
popd

echo "Successfully build kcptun"
