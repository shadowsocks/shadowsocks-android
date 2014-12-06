/*
 * Shadowsocks - A shadowsocks client for Android
 * Copyright (C) 2014 <max.c.lv@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *                            ___====-_  _-====___
 *                      _--^^^#####//      \\#####^^^--_
 *                   _-^##########// (    ) \\##########^-_
 *                  -############//  |\^^/|  \\############-
 *                _/############//   (@::@)   \\############\_
 *               /#############((     \\//     ))#############\
 *              -###############\\    (oo)    //###############-
 *             -#################\\  / VV \  //#################-
 *            -###################\\/      \//###################-
 *           _#/|##########/\######(   /\   )######/\##########|\#_
 *           |/ |#/\#/\#/\/  \#/\##\  |  |  /##/\#/  \/\#/\#/\#| \|
 *           `  |/  V  V  `   V  \#\| |  | |/#/  V   '  V  V  \|  '
 *              `   `  `      `   / | |  | | \   '      '  '   '
 *                               (  | |  | |  )
 *                              __\ | |  | | /__
 *                             (vvv(VVV)(VVV)vvv)
 *
 *                              HERE BE DRAGONS
 *
 */

package com.github.shadowsocks.utils

object Executable {
  val REDSOCKS = "redsocks"
  val PDNSD = "pdnsd"
  val SS_LOCAL = "ss-local"
  val SS_TUNNEL = "ss-tunnel"
  val IPTABLES = "iptables"
  val TUN2SOCKS = "tun2socks"
}

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

  val isNAT = "isNAT"
  val isRoot = "isRoot"
  val status = "status"
  val proxyedApps = "proxyedApps"
  val route = "route"

  val isRunning = "isRunning"
  val isAutoConnect = "isAutoConnect"

  val isGlobalProxy = "isGlobalProxy"
  val isGFWList = "isGFWList"
  val isBypassApps = "isBypassApps"
  val isTrafficStat = "isTrafficStat"
  val isUdpDns = "isUdpDns"

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
  val STOPPING = 3
  val STOPPED = 4
  def isAvailable(state: Int): Boolean = state != CONNECTED && state != CONNECTING
}

object Action {
  val SERVICE = "com.github.shadowsocks.SERVICE"
  val CLOSE = "com.github.shadowsocks.CLOSE"
  val UPDATE_FRAGMENT = "com.github.shadowsocks.ACTION_UPDATE_FRAGMENT"
  val UPDATE_PREFS = "com.github.shadowsocks.ACTION_UPDATE_PREFS"
}

object Route {
  val ALL = "all"
  val BYPASS_LAN = "bypass-lan"
  val BYPASS_CHN = "bypass-china"
}
