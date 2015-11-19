package com.github.shadowsocks.aidl;

interface IShadowsocksServiceCallback {
  oneway void stateChanged(int state, String msg);
  oneway void trafficUpdated(String txRate, String rxRate,
          String txTotal, String rxTotal);
}
