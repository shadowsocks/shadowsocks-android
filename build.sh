#!/bin/bash

function try () {
"$@" || exit -1
}

# Build native binaries
pushd src/main
try $ANDROID_NDK_HOME/ndk-build -j8
# Clean up old binaries (no longer used)
rm -rf assets/armeabi-v7a
rm -rf assets/x86
popd

# Build kcptun
pushd kcptun
try ./make.bash
popd
