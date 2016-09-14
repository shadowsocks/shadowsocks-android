package com.github.shadowsocks.aidl;

import com.github.shadowsocks.aidl.IShadowsocksServiceCallback;

interface IShadowsocksService {
  int getState();
  String getProfileName();

  oneway void registerCallback(IShadowsocksServiceCallback cb);
  oneway void unregisterCallback(IShadowsocksServiceCallback cb);

  oneway void use(in int profileId);
  void useSync(in int profileId);
}
