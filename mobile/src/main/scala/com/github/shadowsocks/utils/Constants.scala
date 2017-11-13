/*******************************************************************************/
/*                                                                             */
/*  Copyright (C) 2017 by Max Lv <max.c.lv@gmail.com>                          */
/*  Copyright (C) 2017 by Mygod Studio <contact-shadowsocks-android@mygod.be>  */
/*                                                                             */
/*  This program is free software: you can redistribute it and/or modify       */
/*  it under the terms of the GNU General Public License as published by       */
/*  the Free Software Foundation, either version 3 of the License, or          */
/*  (at your option) any later version.                                        */
/*                                                                             */
/*  This program is distributed in the hope that it will be useful,            */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of             */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              */
/*  GNU General Public License for more details.                               */
/*                                                                             */
/*  You should have received a copy of the GNU General Public License          */
/*  along with this program. If not, see <http://www.gnu.org/licenses/>.       */
/*                                                                             */
/*******************************************************************************/

package com.github.shadowsocks.utils

object Key {
  val id = "profileId"
  val name = "profileName"

  val individual = "Proxyed"

  val serviceMode = "serviceMode"
  val modeProxy = "proxy"
  val modeVpn = "vpn"
  val modeTransproxy = "transproxy"
  val portProxy = "portProxy"
  val portLocalDns = "portLocalDns"
  val portTransproxy = "portTransproxy"

  val route = "route"

  val isAutoConnect = "isAutoConnect"

  val proxyApps = "isProxyApps"
  val bypass = "isBypassApps"
  val udpdns = "isUdpDns"
  val ipv6 = "isIpv6"

  val host = "proxy"
  val password = "sitekey"
  val method = "encMethod"
  val remotePort = "remotePortNum"
  val remoteDns = "remoteDns"

  val plugin = "plugin"
  val pluginConfigure = "plugin.configure"

  val dirty = "profileDirty"

  val tfo = "tcp_fastopen"
  val assetUpdateTime = "assetUpdateTime"
}

object Action {
  final val SERVICE = "com.github.shadowsocks.SERVICE"
  final val CLOSE = "com.github.shadowsocks.CLOSE"
  final val RELOAD = "com.github.shadowsocks.RELOAD"

  final val EXTRA_PROFILE_ID = "com.github.shadowsocks.EXTRA_PROFILE_ID"
}
