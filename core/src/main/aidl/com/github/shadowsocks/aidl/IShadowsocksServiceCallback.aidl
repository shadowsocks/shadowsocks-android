package com.github.shadowsocks.aidl;

import com.github.shadowsocks.aidl.TrafficStats;

interface IShadowsocksServiceCallback {
  oneway void stateChanged(int state, String profileName, String msg);
  oneway void trafficUpdated(long profileId, in TrafficStats stats);
  // Traffic data has persisted to database, listener should refetch their data from database
  oneway void trafficPersisted(long profileId);
}
