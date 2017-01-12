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

package com.github.shadowsocks

import java.io.File
import java.net.{Inet6Address, InetAddress}
import java.util.Locale

import android.content._
import android.os._
import android.util.Log
import com.github.shadowsocks.ShadowsocksApplication.app
import com.github.shadowsocks.acl.{AclSyncJob, Acl}
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.utils._
import eu.chainfire.libsuperuser.Shell

import scala.collection.JavaConversions._
import scala.collection.mutable.ArrayBuffer

class ShadowsocksNatService extends BaseService {

  val TAG = "ShadowsocksNatService"

  val CMD_IPTABLES_DNAT_ADD_SOCKS =
    "iptables -t nat -A OUTPUT -p tcp -j DNAT --to-destination 127.0.0.1:8123"

  private var notification: ShadowsocksNotification = _
  val myUid: Int = android.os.Process.myUid()

  var sslocalProcess: GuardedProcess = _
  var sstunnelProcess: GuardedProcess = _
  var redsocksProcess: GuardedProcess = _
  var pdnsdProcess: GuardedProcess = _
  var su: Shell.Interactive = _

  def startShadowsocksDaemon() {
    val conf = if (profile.kcp) {
      ConfigUtils
      .SHADOWSOCKS.formatLocal(Locale.ENGLISH, "127.0.0.1", profile.localPort + 90, profile.localPort,
        profile.password, profile.method, 600)
    } else {
      ConfigUtils
      .SHADOWSOCKS.formatLocal(Locale.ENGLISH, profile.host, profile.remotePort, profile.localPort,
        profile.password, profile.method, 600)
    }
    Utils.printToFile(new File(getApplicationInfo.dataDir + "/ss-local-nat.conf"))(p => {
      p.println(conf)
    })

    val cmd = ArrayBuffer[String](getApplicationInfo.nativeLibraryDir + "/libss-local.so"
          , "-b" , "127.0.0.1"
          , "-t" , "600"
          , "-P", getApplicationInfo.dataDir
          , "-c" , getApplicationInfo.dataDir + "/ss-local-nat.conf")

    if (profile.auth) cmd += "-A"

    if (TcpFastOpen.sendEnabled) cmd += "--fast-open"

    if (profile.route != Acl.ALL) {
      cmd += "--acl"
      cmd += Acl.getPath(profile.route)
    }

    if (BuildConfig.DEBUG) Log.d(TAG, cmd.mkString(" "))
    sslocalProcess = new GuardedProcess(cmd).start()
  }

  def startKcptunDaemon() {
    if (profile.kcpcli == null) profile.kcpcli = ""

    val host = if (profile.host.contains(":")) {
      "[" + profile.host + "]"
    } else {
      profile.host
    }

    val cmd = ArrayBuffer[String](getApplicationInfo.nativeLibraryDir + "/libkcptun.so"
      , "-r", host + ":" + profile.kcpPort
      , "-l", "127.0.0.1:" + (profile.localPort + 90))
    try cmd ++= Utils.translateCommandline(profile.kcpcli) catch {
      case exc: Exception => throw KcpcliParseException(exc)
    }

    if (BuildConfig.DEBUG) Log.d(TAG, cmd.mkString(" "))

    kcptunProcess = new GuardedProcess(cmd).start()
  }

  def startDNSTunnel() {
    if (profile.udpdns) {
      val conf = ConfigUtils
        .SHADOWSOCKS.formatLocal(Locale.ENGLISH, profile.host, profile.remotePort, profile.localPort + 53,
          profile.password, profile.method, 10)
      Utils.printToFile(new File(getApplicationInfo.dataDir + "/ss-tunnel-nat.conf"))(p => {
        p.println(conf)
      })
      val cmd = ArrayBuffer[String](getApplicationInfo.nativeLibraryDir + "/libss-tunnel.so"
        , "-u"
        , "-t" , "10"
        , "-b" , "127.0.0.1"
        , "-l" , (profile.localPort + 53).toString
        , "-L" , if (profile.remoteDns == null) "8.8.8.8:53" else profile.remoteDns + ":53"
        , "-P" , getApplicationInfo.dataDir
        , "-c" , getApplicationInfo.dataDir + "/ss-tunnel-nat.conf")

      if (profile.auth) cmd += "-A"

      if (BuildConfig.DEBUG) Log.d(TAG, cmd.mkString(" "))

      sstunnelProcess = new GuardedProcess(cmd).start()

    } else {
      val conf = if (profile.kcp) {
        ConfigUtils
        .SHADOWSOCKS.formatLocal(Locale.ENGLISH, "127.0.0.1", profile.localPort + 90,
          profile.localPort + 63, profile.password, profile.method, 10)
      } else {
        ConfigUtils
        .SHADOWSOCKS.formatLocal(Locale.ENGLISH, profile.host, profile.remotePort, profile.localPort + 63,
          profile.password, profile.method, 10)
      }
      Utils.printToFile(new File(getApplicationInfo.dataDir + "/ss-tunnel-nat.conf"))(p => {
        p.println(conf)
      })
      val cmdBuf = ArrayBuffer[String](getApplicationInfo.nativeLibraryDir + "/libss-tunnel.so"
        , "-t" , "10"
        , "-b" , "127.0.0.1"
        , "-l" , (profile.localPort + 63).toString
        , "-L" , if (profile.remoteDns == null) "8.8.8.8:53" else profile.remoteDns + ":53"
        , "-P", getApplicationInfo.dataDir
        , "-c" , getApplicationInfo.dataDir + "/ss-tunnel-nat.conf")

      if (profile.auth) cmdBuf += "-A"

      if (BuildConfig.DEBUG) Log.d(TAG, cmdBuf.mkString(" "))

      sstunnelProcess = new GuardedProcess(cmdBuf).start()
    }
  }

  def startDnsDaemon() {

    val reject = if (profile.ipv6) "224.0.0.0/3" else "224.0.0.0/3, ::/0"

    val conf = profile.route match {
      case Acl.BYPASS_CHN | Acl.BYPASS_LAN_CHN | Acl.GFWLIST | Acl.CUSTOM_RULES =>
        ConfigUtils.PDNSD_DIRECT.formatLocal(Locale.ENGLISH, "", getApplicationInfo.dataDir,
          "127.0.0.1", profile.localPort + 53, "114.114.114.114, 223.5.5.5, 1.2.4.8",
          getBlackList, reject, profile.localPort + 63, reject)
      case Acl.CHINALIST =>
        ConfigUtils.PDNSD_DIRECT.formatLocal(Locale.ENGLISH, "", getApplicationInfo.dataDir,
          "127.0.0.1", profile.localPort + 53, "8.8.8.8, 8.8.4.4, 208.67.222.222",
          "", reject, profile.localPort + 63, reject)
      case _ =>
        ConfigUtils.PDNSD_LOCAL.formatLocal(Locale.ENGLISH, "", getApplicationInfo.dataDir,
          "127.0.0.1", profile.localPort + 53, profile.localPort + 63, reject)
    }

    Utils.printToFile(new File(getApplicationInfo.dataDir + "/pdnsd-nat.conf"))(p => {
       p.println(conf)
    })
    val cmd = Array(getApplicationInfo.nativeLibraryDir + "/libpdnsd.so", "-c", getApplicationInfo.dataDir + "/pdnsd-nat.conf")

    if (BuildConfig.DEBUG) Log.d(TAG, cmd.mkString(" "))

    pdnsdProcess = new GuardedProcess(cmd).start()
  }

  def startRedsocksDaemon() {
    val conf = ConfigUtils.REDSOCKS.formatLocal(Locale.ENGLISH, profile.localPort)
    val cmd = Array(getApplicationInfo.nativeLibraryDir + "/libredsocks.so", "-c", getApplicationInfo.dataDir + "/redsocks-nat.conf")
    Utils.printToFile(new File(getApplicationInfo.dataDir + "/redsocks-nat.conf"))(p => {
      p.println(conf)
    })

    if (BuildConfig.DEBUG) Log.d(TAG, cmd.mkString(" "))
    redsocksProcess = new GuardedProcess(cmd).start()
  }

  /** Called when the activity is first created. */
  def handleConnection() {

    startDNSTunnel()
    startRedsocksDaemon()
    startShadowsocksDaemon()

    if (!profile.udpdns) startDnsDaemon()
    if (profile.kcp) startKcptunDaemon()

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
    if (kcptunProcess != null) {
      kcptunProcess.destroy()
      kcptunProcess = null
    }
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

  def setupIptables() {
    val init_sb = new ArrayBuffer[String]
    val http_sb = new ArrayBuffer[String]

    init_sb.append("ulimit -n 4096")
    init_sb.append("iptables -t nat -F OUTPUT")

    val cmd_bypass = "iptables -t nat -A OUTPUT -p tcp -d 0.0.0.0 -j RETURN"
    if (!InetAddress.getByName(profile.host.toUpperCase).isInstanceOf[Inet6Address]) {
      init_sb.append(cmd_bypass.replace("-p tcp -d 0.0.0.0", "-d " + profile.host))
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

  override def startRunner(profile: Profile): Unit = if (su == null)
    su = new Shell.Builder().useSU().setWantSTDERR(true).setWatchdogTimeout(10).open((_, exitCode, _) =>
      if (exitCode == 0) super.startRunner(profile) else {
        if (su != null) {
          su.close()
          su = null
        }
        super.stopRunner(stopService = true, getString(R.string.nat_no_root))
      })

  override def connect() {
    super.connect()

    // Clean up
    killProcesses()

    if (!Utils.isNumeric(profile.host)) Utils.resolve(profile.host, enableIPv6 = true) match {
      case Some(a) => profile.host = a
      case None => throw NameNotResolvedException()
    }

    handleConnection()

    if (profile.route != Acl.ALL && profile.route != Acl.CUSTOM_RULES)
      AclSyncJob.schedule(profile.route)

    changeState(State.CONNECTED)
    notification = new ShadowsocksNotification(this, profile.getName, true)
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
