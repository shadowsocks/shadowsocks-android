#!/bin/bash

release=$1
cp mobile/build/outputs/bundle/release/mobile-release.aab         shadowsocks-"${release}".aab
cp tv/build/outputs/bundle/freedomRelease/tv-freedom-release.aab   shadowsocks-tv-"${release}".aab
