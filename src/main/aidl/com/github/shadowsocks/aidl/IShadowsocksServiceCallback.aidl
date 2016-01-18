package com.github.shadowsocks.aidl;

interface IShadowsocksServiceCallback {
  oneway void stateChanged(int state, String msg);
  oneway void trafficUpdated(long txRate, long rxRate, long txTotal, long rxTotal);
}
