package com.github.shadowsocks.utils

import android.content.{Intent, SharedPreferences}
import com.github.shadowsocks.aidl.Config

object Msg {
  val CONNECT_FINISH = 1
  val CONNECT_SUCCESS = 2
  val CONNECT_FAIL = 3
  val VPN_ERROR = 6
}

object Path {
  val BASE = "/data/data/com.github.shadowsocks/"
}

object Key {
  val profileId = "profileId"
  val profileName = "profileName"

  val proxied = "Proxyed"

  val isRoot = "isRoot"
  val status = "status"
  val proxyedApps = "proxyedApps"

  val isRunning = "isRunning"
  val isAutoConnect = "isAutoConnect"

  val isGlobalProxy = "isGlobalProxy"
  val isGFWList = "isGFWList"
  val isBypassApps = "isBypassApps"
  val isTrafficStat = "isTrafficStat"

  val proxy = "proxy"
  val sitekey = "sitekey"
  val encMethod = "encMethod"
  val remotePort = "remotePort"
  val localPort = "port"
}

object Scheme {
  val APP = "app://"
  val PROFILE = "profile://"
  val SS = "ss"
}

object Mode {
  val NAT = 0
  val VPN = 1
}

object State {
  val INIT = 0
  val CONNECTING = 1
  val CONNECTED = 2
  val STOPPED = 3
  def isAvailable(state: Int): Boolean = state != CONNECTED && state != CONNECTING
}

object Action {
  val SERVICE = "com.github.shadowsocks.SERVICE"
  val CLOSE = "com.github.shadowsocks.CLOSE"
  val UPDATE_FRAGMENT = "com.github.shadowsocks.ACTION_UPDATE_FRAGMENT"
  val UPDATE_PREFS = "com.github.shadowsocks.ACTION_UPDATE_PREFS"
}
