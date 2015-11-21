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
import com.github.shadowsocks.{R, ShadowsocksApplication}
import com.google.android.gms.tagmanager.Container
import com.github.kevinsawicki.http.HttpRequest
import com.github.shadowsocks.aidl.Config

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
      | cache_dir = "/data/data/com.github.shadowsocks";
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

  val PDNSD_BYPASS =
    """
      |global {
      | perm_cache = 2048;
      | cache_dir = "/data/data/com.github.shadowsocks";
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
      | ip = 114.114.114.114, 223.5.5.5;
      | uptest = none;
      | preset = on;
      | include = %s;
      | policy = excluded;
      | timeout = 2;
      |}
      |
      |server {
      | label = "local-server";
      | ip = 127.0.0.1;
      | uptest = none;
      | preset = on;
      | port = %d;
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
      | cache_dir = "/data/data/com.github.shadowsocks";
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
      | reject = %s;
      | reject_policy = fail;
      | reject_recursively = on;
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

  def printToFile(f: java.io.File)(op: java.io.PrintWriter => Unit) {
    val p = new java.io.PrintWriter(f)
    try {
      op(p)
    } finally {
      p.close()
    }
  }

  def refresh(context: Context) {
    val holder = ShadowsocksApplication.containerHolder
    if (holder != null) holder.refresh()
  }

  def getRejectList(context: Context): String = {
    val default = context.getString(R.string.reject)
    try {
      val container = ShadowsocksApplication.containerHolder.getContainer
      val update = container.getString("reject")
      if (update == null || update.isEmpty) default else update
    } catch {
      case ex: Exception => default
    }
  }

  def getBlackList(context: Context): String = {
    val default = context.getString(R.string.black_list)
    try {
      val container = ShadowsocksApplication.containerHolder.getContainer
      val update = container.getString("black_list")
      if (update == null || update.isEmpty) default else update
    } catch {
      case ex: Exception => default
    }
  }

  def getPublicConfig(context: Context, container: Container, config: Config): Config = {
    val url = container.getString("proxy_url")
    val sig = Utils.getSignature(context)
    val list = HttpRequest
      .post(url)
      .connectTimeout(2000)
      .readTimeout(2000)
      .send("sig="+sig)
      .body
    val proxies = util.Random.shuffle(list.split('|').toSeq)
    val proxy = proxies.head.split(':')

    val host = proxy(0).trim
    val port = proxy(1).trim.toInt
    val password = proxy(2).trim
    val method = proxy(3).trim

    new Config(config.isProxyApps, config.isBypassApps, config.isUdpDns, config.isAuth, config.isIpv6,
      config.profileName, host, password, method, config.proxiedAppString, config.route, port, config.localPort, 0)
  }

  def load(settings: SharedPreferences): Config = {
    val isProxyApps = settings.getBoolean(Key.isProxyApps, false)
    val isBypassApps = settings.getBoolean(Key.isBypassApps, false)
    val isUdpDns = settings.getBoolean(Key.isUdpDns, false)
    val isAuth = settings.getBoolean(Key.isAuth, false)
    val isIpv6 = settings.getBoolean(Key.isIpv6, false)

    val profileName = settings.getString(Key.profileName, "default")
    val proxy = settings.getString(Key.proxy, "127.0.0.1")
    val sitekey = settings.getString(Key.sitekey, "default")
    val encMethod = settings.getString(Key.encMethod, "table")
    val route = settings.getString(Key.route, "all")

    val remotePort = settings.getInt(Key.remotePort, 1984)
    val localPort = settings.getInt(Key.localPort, 1984)
    val proxiedAppString = settings.getString(Key.proxied, "")

    val profileId = settings.getInt(Key.profileId, -1)

    new Config(isProxyApps, isBypassApps, isUdpDns, isAuth, isIpv6, profileName, proxy, sitekey, encMethod,
      proxiedAppString, route, remotePort, localPort, profileId)
  }
}
