#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
TARGET=$DIR/../main/jni/overture
DEPS=$DIR/.deps

rm -rf $DEPS
rm -rf $DIR/go/bin
rm -rf $DIR/bin
rm -rf $TARGET

echo "Successfully clean overture"
