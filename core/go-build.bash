#!/bin/bash

[[ -z "${ANDROID_NDK_HOME}" ]] && ANDROID_NDK_HOME="${ANDROID_HOME}/ndk-bundle"
TOOLCHAIN="$(find ${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/* -maxdepth 1 -type d -print -quit)/bin"
ABIS=(armeabi-v7a arm64-v8a x86 x86_64)
GO_ARCHS=('arm GOARM=7' arm64 386 amd64)
CLANG_ARCHS=(armv7a-linux-androideabi aarch64-linux-android i686-linux-android x86_64-linux-android)
STRIP_ARCHS=(arm-linux-androideabi aarch64-linux-android i686-linux-android x86_64-linux-android)

MIN_API="$1"
ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
OUT_DIR="$ROOT/build/go"

cd "$ROOT/src/main/jni/overture/main"
BIN="liboverture.so"
for i in "${!ABIS[@]}"; do
    ABI="${ABIS[$i]}"
    [[ -f "${OUT_DIR}/${ABI}/${BIN}" ]] && continue
    echo "Build ${BIN} ${ABI}"
    mkdir -p ${OUT_DIR}/${ABI} \
    && env \
        CGO_ENABLED=1 CC="${TOOLCHAIN}/${CLANG_ARCHS[$i]}${MIN_API}-clang" \
        GOOS=android GOARCH=${GO_ARCHS[$i]} \
        go build -v -ldflags='-s -w' -o "${OUT_DIR}/unstripped" \
    && "${TOOLCHAIN}/${STRIP_ARCHS[$i]}-strip" "${OUT_DIR}/unstripped" -o "${OUT_DIR}/${ABI}/${BIN}" \
    || exit -1
    rm "${OUT_DIR}/unstripped"
done

cd "$ROOT"
