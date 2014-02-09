package com.github.shadowsocks.aidl;

import com.github.shadowsocks.aidl.Config;
import com.github.shadowsocks.aidl.IShadowsocksServiceCallback;

interface IShadowsocksService {
  int getMode();
  int getState();
  void start(in Config config);
  void stop();
  void registerCallback(IShadowsocksServiceCallback cb);
  void unregisterCallback(IShadowsocksServiceCallback cb);
}