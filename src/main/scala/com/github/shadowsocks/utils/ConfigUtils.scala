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

import android.content.{SharedPreferences, Context}
import com.github.shadowsocks.ShadowsocksApplication
import com.google.tagmanager.Container
import scalaj.http.{HttpOptions, Http}
import com.github.shadowsocks.aidl.Config

object ConfigUtils {
  val SHADOWSOCKS = "{\"server\": [%s], \"server_port\": %d, \"local_port\": %d, \"password\": %s, \"timeout\": %d}"
  val REDSOCKS = "base {" +
    " log_debug = off;" +
    " log_info = off;" +
    " log = stderr;" +
    " daemon = on;" +
    " redirector = iptables;" +
    "}" +
    "redsocks {" +
    " local_ip = 127.0.0.1;" +
    " local_port = 8123;" +
    " ip = 127.0.0.1;" +
    " port = %d;" +
    " type = socks5;" +
    "}"
  val PDNSD =
    """
      |global {
      | perm_cache = 2048;
      | cache_dir = "/data/data/com.github.shadowsocks";
      | server_ip = %s;
      | server_port = 8153;
      | query_method = tcp_only;
      | run_ipv4 = on;
      | min_ttl = 15m;
      | max_ttl = 1w;
      | timeout = 10;
      | daemon = on;
      | pid_file = "/data/data/com.github.shadowsocks/pdnsd.pid";
      |}
      |
      |server {
      | label = "google-servers";
      | ip = 8.8.8.8, 8.8.4.4;
	  | exclude = %s;
      | timeout = 5;
      |}
	  |
      |server {
      | label = "china-servers";
      | ip = %s, 114.114.114.114;
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

  def printToFile(f: java.io.File)(op: java.io.PrintWriter => Unit) {
    val p = new java.io.PrintWriter(f)
    try {
      op(p)
    } finally {
      p.close()
    }
  }

  def refresh(context: Context) {
    val container = context.getApplicationContext.asInstanceOf[ShadowsocksApplication].tagContainer
    if (container != null) container.refresh()
  }

  def getPublicConfig(context: Context, container: Container, config: Config): Config = {
    val url = container.getString("proxy_url")
    val sig = Utils.getSignature(context)
    val list = Http
      .post(url)
      .params("sig" -> sig)
      .option(HttpOptions.connTimeout(1000))
      .option(HttpOptions.readTimeout(5000))
      .asString
    val proxies = util.Random.shuffle(list.split('|').toSeq).toSeq
    val proxy = proxies(0).split(':')

    val host = proxy(0).trim
    val port = proxy(1).trim.toInt
    val password = proxy(2).trim
    val method = proxy(3).trim

    new Config(config.isGlobalProxy, config.isGFWList, config.isBypassApps, config.isTrafficStat,
      config.isUdpDns, config.profileName, host, password, method, config.proxiedAppString, port,
      config.localPort)
  }

  def load(settings: SharedPreferences): Config = {
    val isGlobalProxy = settings.getBoolean(Key.isGlobalProxy, false)
    val isGFWList = settings.getBoolean(Key.isGFWList, false)
    val isBypassApps = settings.getBoolean(Key.isBypassApps, false)
    val isTrafficStat = settings.getBoolean(Key.isTrafficStat, false)
    val isUdpDns = settings.getBoolean(Key.isUdpDns, false)

    val profileName = settings.getString(Key.profileName, "default")
    val proxy = settings.getString(Key.proxy, "127.0.0.1")
    val sitekey = settings.getString(Key.sitekey, "default")
    val encMethod = settings.getString(Key.encMethod, "table")
    val remotePort: Int = try {
      settings.getString(Key.remotePort, "1984").toInt
    } catch {
      case ex: NumberFormatException =>
        1984
    }
    val localPort: Int = try {
      settings.getString(Key.localPort, "1984").toInt
    } catch {
      case ex: NumberFormatException =>
        1984
    }
    val proxiedAppString = settings.getString(Key.proxied, "")

    new Config(isGlobalProxy, isGFWList, isBypassApps, isTrafficStat, isUdpDns, profileName, proxy,
      sitekey, encMethod, proxiedAppString, remotePort, localPort)
  }
}
