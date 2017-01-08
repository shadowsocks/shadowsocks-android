#!/bin/bash

function try () {
"$@" || exit -1
}

pushd src/main
# Clean up old binaries (no longer used)
rm -rf assets/armeabi-v7a assets/x86 libs/armeabi-v7a libs/x86
mkdir -p libs/armeabi-v7a libs/x86
popd

# Build kcptun
pushd kcptun
try ./make.bash
popd
