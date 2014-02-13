package com.github.shadowsocks.aidl;

import com.github.shadowsocks.aidl.Config;
import com.github.shadowsocks.aidl.IShadowsocksServiceCallback;

interface IShadowsocksService {
  int getMode();
  int getState();

  void registerCallback(IShadowsocksServiceCallback cb);
  void unregisterCallback(IShadowsocksServiceCallback cb);

  oneway void start(in Config config);
  oneway void stop();
}
