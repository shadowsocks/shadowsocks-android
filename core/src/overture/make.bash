#!/bin/bash

function try () {
"$@" || exit -1
}

[ -z "$ANDROID_NDK_HOME" ] && ANDROID_NDK_HOME=$ANDROID_HOME/ndk-bundle
TOOLCHAIN=$(find $ANDROID_NDK_HOME/toolchains/llvm/prebuilt/* -maxdepth 1 -type d -print -quit)/bin

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
MIN_API=$1
TARGET=$DIR/bin

try mkdir -p $TARGET/armeabi-v7a $TARGET/x86 $TARGET/arm64-v8a $TARGET/x86_64

export GOPATH=$DIR

if [ ! -f "$TARGET/armeabi-v7a/liboverture.so" ] || [ ! -f "$TARGET/arm64-v8a/liboverture.so" ] ||
   [ ! -f "$TARGET/x86/liboverture.so" ] || [ ! -f "$TARGET/x86_64/liboverture.so" ]; then

    pushd $GOPATH/src/github.com/shadowsocks/overture/main

    echo "Get dependences for overture"
    go get -v github.com/shadowsocks/overture/main

    echo "Cross compile overture for arm"
    if [ ! -f "$TARGET/armeabi-v7a/liboverture.so" ]; then
        try env CGO_ENABLED=1 CC=$TOOLCHAIN/armv7a-linux-androideabi${MIN_API}-clang GOOS=android GOARCH=arm GOARM=7 go build -ldflags="-s -w"
        try $TOOLCHAIN/arm-linux-androideabi-strip main
        try mv main $TARGET/armeabi-v7a/liboverture.so
    fi

    echo "Cross compile overture for arm64"
    if [ ! -f "$TARGET/arm64-v8a/liboverture.so" ]; then
        try env CGO_ENABLED=1 CC=$TOOLCHAIN/aarch64-linux-android${MIN_API}-clang GOOS=android GOARCH=arm64 go build -ldflags="-s -w"
        try $TOOLCHAIN/aarch64-linux-android-strip main
        try mv main $TARGET/arm64-v8a/liboverture.so
    fi

    echo "Cross compile overture for 386"
    if [ ! -f "$TARGET/x86/liboverture.so" ]; then
        try env CGO_ENABLED=1 CC=$TOOLCHAIN/i686-linux-android${MIN_API}-clang GOOS=android GOARCH=386 go build -ldflags="-s -w"
        try $TOOLCHAIN/i686-linux-android-strip main
        try mv main $TARGET/x86/liboverture.so
    fi

    echo "Cross compile overture for amd64"
    if [ ! -f "$TARGET/x86_64/liboverture.so" ]; then
        try env CGO_ENABLED=1 CC=$TOOLCHAIN/x86_64-linux-android${MIN_API}-clang GOOS=android GOARCH=amd64 go build -ldflags="-s -w"
        try $TOOLCHAIN/x86_64-linux-android-strip main
        try mv main $TARGET/x86_64/liboverture.so
    fi

    popd

fi

echo "Successfully build overture"
