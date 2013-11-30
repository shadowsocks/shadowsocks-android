package com.github.shadowsocks.utils

import android.content.{Intent, SharedPreferences}

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
  val SHADOW = "shadow:"
}

object State {
  val INIT = 0
  val CONNECTING = 1
  val CONNECTED = 2
  val STOPPED = 3
}

object Action {
  val CLOSE = "com.github.shadowsocks.ACTION_SHUTDOWN"
  val UPDATE_STATE = "com.github.shadowsocks.ACTION_UPDATE_STATE"
  val UPDATE_FRAGMENT = "com.github.shadowsocks.ACTION_UPDATE_FRAGMENT"
  val UPDATE_PREFS = "com.github.shadowsocks.ACTION_UPDATE_PREFS"
}

object Extra {
  val STATE = "state"
  val MESSAGE = "message"

  def save(settings: SharedPreferences, config: Config) {
    val edit = settings.edit()

    edit.putBoolean(Key.isGlobalProxy, config.isGlobalProxy)
    edit.putBoolean(Key.isGFWList, config.isGFWList)
    edit.putBoolean(Key.isBypassApps, config.isBypassApps)
    edit.putBoolean(Key.isTrafficStat, config.isTrafficStat)

    edit.putString(Key.profileName, config.profileName)
    edit.putString(Key.proxy, config.proxy)
    edit.putString(Key.sitekey, config.sitekey)
    edit.putString(Key.encMethod, config.encMethod)
    edit.putString(Key.remotePort, config.remotePort.toString)
    edit.putString(Key.localPort, config.localPort.toString)

    edit.apply()
  }

  def get(intent: Intent): Config = {
    val isGlobalProxy = intent.getBooleanExtra(Key.isGlobalProxy, false)
    val isGFWList = intent.getBooleanExtra(Key.isGFWList, false)
    val isBypassApps = intent.getBooleanExtra(Key.isBypassApps, false)
    val isTrafficStat = intent.getBooleanExtra(Key.isTrafficStat, false)

    val profileName = intent.getStringExtra(Key.profileName)
    val proxy = intent.getStringExtra(Key.proxy)
    val sitekey = intent.getStringExtra(Key.sitekey)
    val encMethod = intent.getStringExtra(Key.encMethod)
    val remotePort = intent.getIntExtra(Key.remotePort, 1984)
    val localPort = intent.getIntExtra(Key.localPort, 1984)
    val proxiedString = intent.getStringExtra(Key.proxied)

    new Config(isGlobalProxy, isGFWList, isBypassApps, isTrafficStat, profileName, proxy, sitekey,
      encMethod, remotePort, localPort, proxiedString)
  }

  def put(settings: SharedPreferences, intent: Intent) {
    val isGlobalProxy = settings.getBoolean(Key.isGlobalProxy, false)
    val isGFWList = settings.getBoolean(Key.isGFWList, false)
    val isBypassApps = settings.getBoolean(Key.isBypassApps, false)
    val isTrafficStat = settings.getBoolean(Key.isTrafficStat, false)


    val profileName = settings.getString(Key.profileName, "default")
    val proxy = settings.getString(Key.proxy, "127.0.0.1")
    val sitekey = settings.getString(Key.sitekey, "default")
    val encMethod = settings.getString(Key.encMethod, "table")
    val remotePort: Int = try {
      settings.getString(Key.remotePort, "1984").toInt
    } catch {
      case ex: NumberFormatException => {
        1984
      }
    }
    val localProt: Int = try {
      settings.getString(Key.localPort, "1984").toInt
    } catch {
      case ex: NumberFormatException => {
        1984
      }
    }
    val proxiedAppString = settings.getString(Key.proxied, "")

    intent.putExtra(Key.isGlobalProxy, isGlobalProxy)
    intent.putExtra(Key.isGFWList, isGFWList)
    intent.putExtra(Key.isBypassApps, isBypassApps)
    intent.putExtra(Key.isTrafficStat, isTrafficStat)

    intent.putExtra(Key.profileName, profileName)
    intent.putExtra(Key.proxy, proxy)
    intent.putExtra(Key.sitekey, sitekey)
    intent.putExtra(Key.encMethod, encMethod)
    intent.putExtra(Key.remotePort, remotePort)
    intent.putExtra(Key.localPort, localProt)

    intent.putExtra(Key.proxied, proxiedAppString)
  }
}

