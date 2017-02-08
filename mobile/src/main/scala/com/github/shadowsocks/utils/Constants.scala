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

object Executable {
  val REDSOCKS = "redsocks"
  val PDNSD = "pdnsd"
  val SS_LOCAL = "ss-local"
  val SS_TUNNEL = "ss-tunnel"
  val TUN2SOCKS = "tun2socks"

  val EXECUTABLES = Array(SS_LOCAL, SS_TUNNEL, PDNSD, REDSOCKS, TUN2SOCKS)
}

object ConfigUtils {
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
      | %s
      | cache_dir = "%s";
      | server_ip = %s;
      | server_port = %d;
      | query_method = tcp_only;
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
      | reject = %s;
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

  val PDNSD_DIRECT =
    """
      |global {
      | perm_cache = 2048;
      | %s
      | cache_dir = "%s";
      | server_ip = %s;
      | server_port = %d;
      | query_method = udp_only;
      | min_ttl = 15m;
      | max_ttl = 1w;
      | timeout = 10;
      | daemon = off;
      | par_queries = 4;
      |}
      |
      |server {
      | label = "remote-servers";
      | ip = %s;
      | timeout = 3;
      | query_method = udp_only;
      | %s
      | policy = included;
      | reject = %s;
      | reject_policy = fail;
      | reject_recursively = on;
      |}
      |
      |server {
      | label = "local-server";
      | ip = 127.0.0.1;
      | query_method = tcp_only;
      | port = %d;
      | reject = %s;
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
  val ipv6 = "isIpv6"

  val host = "proxy"
  val password = "sitekey"
  val method = "encMethod"
  val remotePort = "remotePortNum"
  val localPort = "localPortNum"
  val remoteDns = "remoteDns"

  val plugin = "plugin"
  val pluginConfigure = "plugin.configure"

  val dirty = "profileDirty"

  val tfo = "tcp_fastopen"
  val currentVersionCode = "currentVersionCode"
}

object State {
  /**
    * This state will never be broadcast by the service. This state is only used to indicate that the current context
    * hasn't bound to any context.
    */
  val IDLE = 0
  val CONNECTING = 1
  val CONNECTED = 2
  val STOPPING = 3
  val STOPPED = 4
}

object Action {
  final val SERVICE = "com.github.shadowsocks.SERVICE"
  final val CLOSE = "com.github.shadowsocks.CLOSE"

  final val EXTRA_PROFILE_ID = "com.github.shadowsocks.EXTRA_PROFILE_ID"
}
