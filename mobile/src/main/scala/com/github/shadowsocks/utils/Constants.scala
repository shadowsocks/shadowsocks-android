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

  val OVERTURE_DIRECT =
    """
    |{
    |  "BindAddress": ":%d",
    |  "PrimaryDNS": [
    |    {
    |      "Name": "Primary-1",
    |      "Address": "%s:53",
    |      "Protocol": "%s",
    |      "Socks5Address": "",
    |      "Timeout": 6,
    |      "EDNSClientSubnet": {
    |        "Policy": "disable",
    |        "ExternalIP": ""
    |      }
    |    },
    |    {
    |      "Name": "Primary-2",
    |      "Address": "%s:53",
    |      "Protocol": "%s",
    |      "Socks5Address": "",
    |      "Timeout": 6,
    |      "EDNSClientSubnet": {
    |        "Policy": "disable",
    |        "ExternalIP": ""
    |      }
    |    }
    |  ],
    |  "AlternativeDNS":[
    |    {
    |      "Name": "Alternative",
    |      "Address": "%s:53",
    |      "Protocol": "tcp",
    |      "Socks5Address": "%s",
    |      "Timeout": 6,
    |      "EDNSClientSubnet":{
    |        "Policy": "auto",
    |        "ExternalIP": "%s"
    |      }
    |   }
    |  ],
    |  "RedirectIPv6Record": true,
    |  "IPNetworkFile": "china_ip_list.txt",
    |  "DomainFile": "gfwlist.txt",
    |  "DomainBase64Decode": true,
    |  "HostsFile": "hosts",
    |  "MinimumTTL": 3600,
    |  "CacheSize" : 4096
    |}
    """.stripMargin

  val OVERTURE_LOCAL =
    """
    |{
    |  "BindAddress": ":%d",
    |  "PrimaryDNS": [
    |    {
    |      "Name": "Primary",
    |      "Address": "%s:53",
    |      "Protocol": "tcp",
    |      "Socks5Address": "%s",
    |      "Timeout": 6,
    |      "EDNSClientSubnet": {
    |        "Policy": "auto",
    |        "ExternalIP": "%s"
    |      }
    |    }
    |  ],
    |  "AlternativeDNS":[
    |    {
    |      "Name": "Alternative",
    |      "Address": "%s:53",
    |      "Protocol": "tcp",
    |      "Socks5Address": "%s",
    |      "Timeout": 6,
    |      "EDNSClientSubnet":{
    |        "Policy": "auto",
    |        "ExternalIP": "%s"
    |      }
    |   }
    |  ],
    |  "RedirectIPv6Record": true,
    |  "IPNetworkFile": "",
    |  "DomainFile": "",
    |  "DomainBase64Decode": true,
    |  "HostsFile": "hosts",
    |  "MinimumTTL": 3600,
    |  "CacheSize" : 4096
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
