package com.github.shadowsocks.aidl;

interface IShadowsocksServiceCallback {
  void stateChanged(int state, String msg);
}