#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
DEPS=$DIR/.deps

rm -rf $DEPS
rm -rf $DIR/go/bin
rm -rf $DIR/bin

echo "Successfully clean overture"
