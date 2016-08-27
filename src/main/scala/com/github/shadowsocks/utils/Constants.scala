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
  val TUN2SOCKS = "tun2socks"
  val KCPTUN = "kcptun"
}

object ConfigUtils {
  val SHADOWSOCKS = "{\"server\": \"%s\", \"server_port\": %d, \"local_port\": %d, \"password\": \"%s\", \"method\":\"%s\", \"timeout\": %d}"
  val REDSOCKS = "base {\n" +
    " log_debug = off;\n" +
    " log_info = off;\n" +
    " log = stderr;\n" +
    " daemon = off;\n" +
    " redirector = iptables;\n" +
    "}\n" +
    "redsocks {\n" +
    " local_ip = 127.0.0.1;\n" +
    " local_port = 8123;\n" +
    " ip = 127.0.0.1;\n" +
    " port = %d;\n" +
    " type = socks5;\n" +
    "}\n"

  val PDNSD_LOCAL =
    """
      |global {
      | perm_cache = 2048;
      | cache_dir = "%s";
      | server_ip = %s;
      | server_port = %d;
      | query_method = tcp_only;
      | run_ipv4 = on;
      | min_ttl = 15m;
      | max_ttl = 1w;
      | timeout = 10;
      | daemon = off;
      |}
      |
      |server {
      | label = "local";
      | ip = 127.0.0.1;
      | port = %d;
      | %s
      | reject_policy = negate;
      | reject_recursively = on;
      | timeout = 5;
      |}
      |
      |rr {
      | name=localhost;
      | reverse=on;
      | a=127.0.0.1;
      | owner=localhost;
      | soa=localhost,root.localhost,42,86400,900,86400,86400;
      |}
    """.stripMargin

  val PDNSD_DIRECT =
    """
      |global {
      | perm_cache = 2048;
      | cache_dir = "%s";
      | server_ip = %s;
      | server_port = %d;
      | query_method = tcp_only;
      | run_ipv4 = on;
      | min_ttl = 15m;
      | max_ttl = 1w;
      | timeout = 10;
      | daemon = off;
      |}
      |
      |server {
      | label = "china-servers";
      | ip = 114.114.114.114, 112.124.47.27;
      | timeout = 4;
      | exclude = %s;
      | policy = included;
      | uptest = none;
      | preset = on;
      |}
      |
      |server {
      | label = "local-server";
      | ip = 127.0.0.1;
      | port = %d;
      | %s
      | reject_policy = negate;
      | reject_recursively = on;
      |}
      |
      |rr {
      | name=localhost;
      | reverse=on;
      | a=127.0.0.1;
      | owner=localhost;
      | soa=localhost,root.localhost,42,86400,900,86400,86400;
      |}
    """.stripMargin
}

object Key {
  val id = "profileId"
  val name = "profileName"

  val individual = "Proxyed"

  val isNAT = "isNAT"
  val route = "route"

  val isAutoConnect = "isAutoConnect"

  val proxyApps = "isProxyApps"
  val bypass = "isBypassApps"
  val udpdns = "isUdpDns"
  val auth = "isAuth"
  val ipv6 = "isIpv6"

  val host = "proxy"
  val password = "sitekey"
  val method = "encMethod"
  val remotePort = "remotePortNum"
  val localPort = "localPortNum"

  val profileTip = "profileTip"

  val kcp = "kcp"
  val kcpPort = "kcpPort"
  val kcpcli = "kcpcli"

  val tfo = "tcp_fastopen"
}

object State {
  val CONNECTING = 1
  val CONNECTED = 2
  val STOPPING = 3
  val STOPPED = 4
  def isAvailable(state: Int): Boolean = state != CONNECTED && state != CONNECTING
}

object Action {
  val SERVICE = "com.github.shadowsocks.SERVICE"
  val CLOSE = "com.github.shadowsocks.CLOSE"
  val QUICK_SWITCH = "com.github.shadowsocks.QUICK_SWITCH"
}

object Route {
  val ALL = "all"
  val BYPASS_LAN = "bypass-lan"
  val BYPASS_CHN = "bypass-china"
  val BYPASS_LAN_CHN = "bypass-lan-china"
}
