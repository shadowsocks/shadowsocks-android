#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

rm -rf $DIR/.deps
rm -rf $DIR/bin

echo "Successfully clean overture"
