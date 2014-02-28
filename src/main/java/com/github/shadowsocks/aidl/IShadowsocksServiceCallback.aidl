package com.github.shadowsocks.aidl;

interface IShadowsocksServiceCallback {
  oneway void stateChanged(int state, String msg);
}
