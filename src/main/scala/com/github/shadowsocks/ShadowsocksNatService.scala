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

package com.github.shadowsocks

import java.io.File
import java.net.{Inet6Address, InetAddress}
import java.util.Locale
import scala.io.Source

import android.content._
import android.os._
import android.system.Os
import android.util.Log
import com.github.shadowsocks.ShadowsocksApplication.app
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.job.AclSyncJob
import com.github.shadowsocks.utils._
import eu.chainfire.libsuperuser.Shell
import android.preference.PreferenceManager
import java.io.RandomAccessFile

import scala.collection.JavaConversions._
import scala.collection.mutable.ArrayBuffer

class ShadowsocksNatService extends BaseService {

  val TAG = "ShadowsocksNatService"

  val CMD_IPTABLES_DNAT_ADD_SOCKS = "iptables -t nat -A OUTPUT -p tcp " +
    "-j DNAT --to-destination 127.0.0.1:8123"

  private var notification: ShadowsocksNotification = _
  val myUid = android.os.Process.myUid()

  var sslocalProcess: GuardedProcess = _
  var sstunnelProcess: GuardedProcess = _
  var redsocksProcess: GuardedProcess = _
  var pdnsdProcess: GuardedProcess = _
  var su: Shell.Interactive = _
  var proxychains_enable: Boolean = false
  var host_arg = ""
  var dns_address = ""
  var dns_port = 0
  var china_dns_address = ""
  var china_dns_port = 0

  def startShadowsocksDaemon() {

    val conf = ConfigUtils
      .SHADOWSOCKS.formatLocal(Locale.ENGLISH, profile.host, profile.remotePort, profile.localPort,
        ConfigUtils.EscapedJson(profile.password), profile.method, 600, profile.protocol, profile.obfs, ConfigUtils.EscapedJson(profile.obfs_param), ConfigUtils.EscapedJson(profile.protocol_param))
    Utils.printToFile(new File(getApplicationInfo.dataDir + "/ss-local-nat.conf"))(p => {
      p.println(conf)
    })

    val cmd = ArrayBuffer[String](getApplicationInfo.dataDir + "/ss-local", "-x"
          , "-b" , "127.0.0.1"
          , "-t" , "600"
          , "--host", host_arg
          , "-P", getApplicationInfo.dataDir
          , "-c" , getApplicationInfo.dataDir + "/ss-local-nat.conf")

    if (TcpFastOpen.sendEnabled) cmd += "--fast-open"

    if (profile.route != Route.ALL) {
      cmd += "--acl"
      cmd += getApplicationInfo.dataDir + '/' + profile.route + ".acl"
    }

    if (proxychains_enable) {
      cmd prepend "LD_PRELOAD=" + getApplicationInfo.dataDir + "/lib/libproxychains4.so"
      cmd prepend "PROXYCHAINS_CONF_FILE=" + getApplicationInfo.dataDir + "/proxychains.conf"
      cmd prepend "PROXYCHAINS_PROTECT_FD_PREFIX=" + getApplicationInfo.dataDir
      cmd prepend "env"
    }

    if (BuildConfig.DEBUG) Log.d(TAG, cmd.mkString(" "))
    sslocalProcess = new GuardedProcess(cmd).start()
  }

  def startTunnel() {
    var localPort = profile.localPort + 63
    if (profile.udpdns) {
      localPort = profile.localPort + 53
    }

    val conf = ConfigUtils
      .SHADOWSOCKS.formatLocal(Locale.ENGLISH, profile.host, profile.remotePort, localPort,
        ConfigUtils.EscapedJson(profile.password), profile.method, 600, profile.protocol, profile.obfs, ConfigUtils.EscapedJson(profile.obfs_param), ConfigUtils.EscapedJson(profile.protocol_param))
    Utils.printToFile(new File(getApplicationInfo.dataDir + "/ss-tunnel-nat.conf"))(p => {
      p.println(conf)
    })
    val cmd = ArrayBuffer[String](getApplicationInfo.dataDir + "/ss-local"
      , "-u"
      , "-t" , "60"
      , "--host", host_arg
      , "-b" , "127.0.0.1"
      , "-l" , localPort.toString
      , "-P" , getApplicationInfo.dataDir
      , "-c" , getApplicationInfo.dataDir + "/ss-tunnel-nat.conf")

    cmd += "-L"
    if (profile.route == Route.CHINALIST)
      cmd += china_dns_address + ":" + china_dns_port.toString
    else
      cmd += dns_address + ":" + dns_port.toString

    if (proxychains_enable) {
      cmd prepend "LD_PRELOAD=" + getApplicationInfo.dataDir + "/lib/libproxychains4.so"
      cmd prepend "PROXYCHAINS_CONF_FILE=" + getApplicationInfo.dataDir + "/proxychains.conf"
      cmd prepend "PROXYCHAINS_PROTECT_FD_PREFIX=" + getApplicationInfo.dataDir
      cmd prepend "env"
    }

    if (BuildConfig.DEBUG) Log.d(TAG, cmd.mkString(" "))

    sstunnelProcess = new GuardedProcess(cmd).start()
  }

  def startDnsDaemon() {

    val reject = if (profile.ipv6) "224.0.0.0/3" else "224.0.0.0/3, ::/0"

    var china_dns_settings = ""

    var remote_dns = false

    if (profile.route == Route.ACL) {
        //decide acl route
        val total_lines = Source.fromFile(new File(getApplicationInfo.dataDir + '/' + profile.route + ".acl")).getLines()
        total_lines.foreach((line: String) => {
            if (line.equals("[remote_dns]")) {
                remote_dns = true
            }
        })
    }

    val black_list = profile.route match {
      case Route.BYPASS_CHN | Route.BYPASS_LAN_CHN | Route.GFWLIST => {
        getBlackList
      }
      case Route.ACL => {
        if (remote_dns) {
            ""
        } else {
            getBlackList
        }
      }
      case _ => {
        ""
      }
    }

    for (china_dns <- profile.china_dns.split(",")) {
      china_dns_settings += ConfigUtils.REMOTE_SERVER.formatLocal(Locale.ENGLISH, china_dns.split(":")(0), china_dns.split(":")(1).toInt,
        black_list, reject)
    }

    val conf = profile.route match {
      case Route.BYPASS_CHN | Route.BYPASS_LAN_CHN | Route.GFWLIST => {
        ConfigUtils.PDNSD_DIRECT.formatLocal(Locale.ENGLISH, "", getApplicationInfo.dataDir,
          "127.0.0.1", profile.localPort + 53, china_dns_settings, profile.localPort + 63, reject)
      }
      case Route.CHINALIST => {
        ConfigUtils.PDNSD_DIRECT.formatLocal(Locale.ENGLISH, "", getApplicationInfo.dataDir,
          "127.0.0.1", profile.localPort + 53, china_dns_settings, profile.localPort + 63, reject)
      }
      case Route.ACL => {
        if (!remote_dns) {
            ConfigUtils.PDNSD_DIRECT.formatLocal(Locale.ENGLISH, "", getApplicationInfo.dataDir,
              "127.0.0.1", profile.localPort + 53, china_dns_settings, profile.localPort + 63, reject)
        } else {
            ConfigUtils.PDNSD_LOCAL.formatLocal(Locale.ENGLISH, "", getApplicationInfo.dataDir,
              "127.0.0.1", profile.localPort + 53, profile.localPort + 63, reject)
        }
      }
      case _ => {
        ConfigUtils.PDNSD_LOCAL.formatLocal(Locale.ENGLISH, "", getApplicationInfo.dataDir,
          "127.0.0.1", profile.localPort + 53, profile.localPort + 63, reject)
      }
    }

    Utils.printToFile(new File(getApplicationInfo.dataDir + "/pdnsd-nat.conf"))(p => {
       p.println(conf)
    })
    val cmd = Array(getApplicationInfo.dataDir + "/pdnsd", "-c", getApplicationInfo.dataDir + "/pdnsd-nat.conf")

    if (BuildConfig.DEBUG) Log.d(TAG, cmd.mkString(" "))

    pdnsdProcess = new GuardedProcess(cmd).start()
  }

  def startRedsocksDaemon() {
    val conf = ConfigUtils.REDSOCKS.formatLocal(Locale.ENGLISH, profile.localPort)
    val cmd = Array(getApplicationInfo.dataDir + "/redsocks", "-c", getApplicationInfo.dataDir + "/redsocks-nat.conf")
    Utils.printToFile(new File(getApplicationInfo.dataDir + "/redsocks-nat.conf"))(p => {
      p.println(conf)
    })

    if (BuildConfig.DEBUG) Log.d(TAG, cmd.mkString(" "))
    redsocksProcess = new GuardedProcess(cmd).start()
  }

  /** Called when the activity is first created. */
  def handleConnection() {
    startTunnel()
    if (!profile.udpdns) startDnsDaemon()
    startRedsocksDaemon()
    startShadowsocksDaemon()
    setupIptables()
  }

  def onBind(intent: Intent): IBinder = {
    Log.d(TAG, "onBind")
    if (Action.SERVICE == intent.getAction) {
      binder
    } else {
      null
    }
  }

  def killProcesses() {
    if (sslocalProcess != null) {
      sslocalProcess.destroy()
      sslocalProcess = null
    }
    if (sstunnelProcess != null) {
      sstunnelProcess.destroy()
      sstunnelProcess = null
    }
    if (redsocksProcess != null) {
      redsocksProcess.destroy()
      redsocksProcess = null
    }
    if (pdnsdProcess != null) {
      pdnsdProcess.destroy()
      pdnsdProcess = null
    }

    su.addCommand("iptables -t nat -F OUTPUT")
  }

  def setupIptables() = {
    val init_sb = new ArrayBuffer[String]
    val http_sb = new ArrayBuffer[String]

    init_sb.append("ulimit -n 4096")
    init_sb.append("iptables -t nat -F OUTPUT")

    val cmd_bypass = "iptables -t nat -A OUTPUT -p tcp -d 0.0.0.0 -j RETURN"
    if (!InetAddress.getByName(profile.host.toUpperCase).isInstanceOf[Inet6Address]) {
      if (proxychains_enable) {
        val raf = new RandomAccessFile(getApplicationInfo.dataDir + "/proxychains.conf", "r");
        val len = raf.length();
        var lastLine = "";
        if (len != 0L) {
          var pos = len - 1;
          while (pos > 0) {
            pos -= 1;
            raf.seek(pos);
            if (raf.readByte() == '\n' && lastLine.equals("")) {
              lastLine = raf.readLine();
            }
          }
        }
        raf.close();

        val str_array = lastLine.split(' ')
        var host = str_array(1)

        if (!Utils.isNumeric(host)) Utils.resolve(host, enableIPv6 = true) match {
          case Some(a) => host = a
          case None => throw NameNotResolvedException()
        }

        init_sb.append(cmd_bypass.replace("-p tcp -d 0.0.0.0", "-d " + host))
      } else {
        init_sb.append(cmd_bypass.replace("-p tcp -d 0.0.0.0", "-d " + profile.host))
      }
    }
    init_sb.append(cmd_bypass.replace("-p tcp -d 0.0.0.0", "-d 127.0.0.1"))
    init_sb.append(cmd_bypass.replace("-p tcp -d 0.0.0.0", "-m owner --uid-owner " + myUid))
    init_sb.append(cmd_bypass.replace("-d 0.0.0.0", "--dport 53"))

    init_sb.append("iptables -t nat -A OUTPUT -p udp --dport 53 -j DNAT --to-destination 127.0.0.1:"
      + (profile.localPort + 53))

    if (!profile.proxyApps || profile.bypass) {
      http_sb.append(CMD_IPTABLES_DNAT_ADD_SOCKS)
    }
    if (profile.proxyApps) {
      val uidMap = getPackageManager.getInstalledApplications(0).map(ai => ai.packageName -> ai.uid).toMap
      for (pn <- profile.individual.split('\n')) uidMap.get(pn) match {
        case Some(uid) =>
          if (!profile.bypass) {
            http_sb.append(CMD_IPTABLES_DNAT_ADD_SOCKS
              .replace("-t nat", "-t nat -m owner --uid-owner " + uid))
          } else {
            init_sb.append(cmd_bypass.replace("-d 0.0.0.0", "-m owner --uid-owner " + uid))
          }
        case _ => // probably removed package, ignore
      }
    }
    su.addCommand((init_sb ++ http_sb).toArray)
  }

  override def startRunner(profile: Profile) = if (su == null)
    su = new Shell.Builder().useSU().setWantSTDERR(true).setWatchdogTimeout(10).open((_, exitCode, _) =>
      if (exitCode == 0) super.startRunner(profile) else {
        if (su != null) {
          su.close()
          su = null
        }
        super.stopRunner(true, getString(R.string.nat_no_root))
      })

  override def connect() {
    super.connect()

    // Clean up
    killProcesses()

    if (new File(getApplicationInfo.dataDir + "/proxychains.conf").exists) {
      proxychains_enable = true
      //Os.setenv("PROXYCHAINS_CONF_FILE", getApplicationInfo.dataDir + "/proxychains.conf", true)
      //Os.setenv("PROXYCHAINS_PROTECT_FD_PREFIX", getApplicationInfo.dataDir, true)
    } else {
      proxychains_enable = false
    }

    try {
      val dns = scala.util.Random.shuffle(profile.dns.split(",").toList).head
      dns_address = dns.split(":")(0)
      dns_port = dns.split(":")(1).toInt

      val china_dns = scala.util.Random.shuffle(profile.china_dns.split(",").toList).head
      china_dns_address = china_dns.split(":")(0)
      china_dns_port = china_dns.split(":")(1).toInt
    } catch {
      case ex: Exception =>
        dns_address = "8.8.8.8"
        dns_port = 53

        china_dns_address = "223.5.5.5"
        china_dns_port = 53
    }

    host_arg = profile.host
    if (!Utils.isNumeric(profile.host)) Utils.resolve(profile.host, enableIPv6 = true) match {
      case Some(a) => profile.host = a
      case None => throw NameNotResolvedException()
    }

    handleConnection()

    if (profile.route != Route.ALL)
      AclSyncJob.schedule(profile.route)

    changeState(State.CONNECTED)

    notification = new ShadowsocksNotification(this, profile.name, true)
  }

  override def stopRunner(stopService: Boolean, msg: String = null) {

    if (notification != null) notification.destroy()

    // channge the state
    changeState(State.STOPPING)

    app.track(TAG, "stop")

    // reset NAT
    killProcesses()

    su.close()
    su = null

    super.stopRunner(stopService, msg)
  }
}
