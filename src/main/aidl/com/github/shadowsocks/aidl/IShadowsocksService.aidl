package com.github.shadowsocks.aidl;

import com.github.shadowsocks.aidl.Config;
import com.github.shadowsocks.aidl.IShadowsocksServiceCallback;

interface IShadowsocksService {
  int getMode();
  int getState();

  oneway void registerCallback(IShadowsocksServiceCallback cb);
  oneway void unregisterCallback(IShadowsocksServiceCallback cb);

  oneway void start(in Config config);
  oneway void stop();
}
