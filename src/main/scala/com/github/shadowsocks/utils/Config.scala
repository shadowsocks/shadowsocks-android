package com.github.shadowsocks.utils

import android.content.Context
import com.github.shadowsocks.ShadowsocksApplication
import com.google.tagmanager.Container
import scalaj.http.{HttpOptions, Http}

object Config {
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
      | label = "root-servers";
      | ip = 8.8.8.8, 8.8.4.4, 208.67.222.222, 208.67.220.220;
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
    val list = Http.post(url)
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
      config.profileName, host, password, method, port, config.localPort, config.proxiedAppString)
  }
}

case class Config(isGlobalProxy: Boolean, isGFWList: Boolean, isBypassApps: Boolean,
  isTrafficStat: Boolean, profileName: String, var proxy: String, sitekey: String,
  encMethod: String, remotePort: Int, localPort: Int, proxiedAppString: String)
