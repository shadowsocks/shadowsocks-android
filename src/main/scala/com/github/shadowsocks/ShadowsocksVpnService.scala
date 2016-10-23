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
import java.util.Locale

import android.annotation.SuppressLint
import android.content._
import android.content.pm.PackageManager.NameNotFoundException
import android.net.VpnService
import android.os._
import android.util.Log
import com.github.shadowsocks.ShadowsocksApplication.app
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.job.AclSyncJob
import com.github.shadowsocks.utils._

import scala.collection.mutable.ArrayBuffer

class ShadowsocksVpnService extends VpnService with BaseService {
  val TAG = "ShadowsocksVpnService"
  val VPN_MTU = 1500
  val PRIVATE_VLAN = "26.26.26.%s"
  val PRIVATE_VLAN6 = "fdfe:dcba:9876::%s"
  var conn: ParcelFileDescriptor = _
  var vpnThread: ShadowsocksVpnThread = _
  private var notification: ShadowsocksNotification = _

  var sslocalProcess: GuardedProcess = _
  var sstunnelProcess: GuardedProcess = _
  var pdnsdProcess: GuardedProcess = _
  var tun2socksProcess: GuardedProcess = _

  override def onBind(intent: Intent): IBinder = {
    val action = intent.getAction
    if (VpnService.SERVICE_INTERFACE == action) {
      return super.onBind(intent)
    } else if (Action.SERVICE == action) {
      return binder
    }
    null
  }

  override def onRevoke() {
    stopRunner(true)
  }

  override def stopRunner(stopService: Boolean, msg: String = null) {

    if (vpnThread != null) {
      vpnThread.stopThread()
      vpnThread = null
    }

    if (notification != null) notification.destroy()

    // channge the state
    changeState(State.STOPPING)

    app.track(TAG, "stop")

    // reset VPN
    killProcesses()

    // close connections
    if (conn != null) {
      conn.close()
      conn = null
    }

    super.stopRunner(stopService, msg)
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
    if (tun2socksProcess != null) {
      tun2socksProcess.destroy()
      tun2socksProcess = null
    }
    if (pdnsdProcess != null) {
      pdnsdProcess.destroy()
      pdnsdProcess = null
    }
  }

  override def startRunner(profile: Profile) {

    // ensure the VPNService is prepared
    if (VpnService.prepare(this) != null) {
      val i = new Intent(this, classOf[ShadowsocksRunnerActivity])
      i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
      startActivity(i)
      stopRunner(true)
      return
    }

    super.startRunner(profile)
  }

  override def connect() = {
    super.connect()

    vpnThread = new ShadowsocksVpnThread(this)
    vpnThread.start()

    // reset the context
    killProcesses()

    // Resolve the server address
    if (!Utils.isNumeric(profile.host)) Utils.resolve(profile.host, enableIPv6 = true) match {
      case Some(addr) => profile.host = addr
      case None => throw NameNotResolvedException()
    }

    handleConnection()
    changeState(State.CONNECTED)

    if (profile.route != Route.ALL)
      AclSyncJob.schedule(profile.route)

    notification = new ShadowsocksNotification(this, profile.name)
  }

  /** Called when the activity is first created. */
  def handleConnection() {
    
    val fd = startVpn()
    if (!sendFd(fd)) throw new Exception("sendFd failed")

    if (profile.kcp) {
      startKcptunDaemon()
    }

    startShadowsocksDaemon()

    if (profile.udpdns && profile.kcp) {
      startShadowsocksUDPDaemon()
    }

    if (!profile.udpdns) {
      startDnsDaemon()
      startDnsTunnel()
    }
  }

  def startKcptunDaemon() {
    if (profile.kcpcli == null) profile.kcpcli = ""

    val host = if (profile.host.contains(":")) {
      "[" + profile.host + "]"
    } else {
      profile.host
    }

    val cmd = ArrayBuffer(getApplicationInfo.dataDir + "/kcptun"
      , "-r", host + ":" + profile.kcpPort
      , "-l", "127.0.0.1:" + (profile.localPort + 90)
      , "--path", protectPath)
    try cmd ++= Utils.translateCommandline(profile.kcpcli) catch {
      case exc: Exception => throw KcpcliParseException(exc)
    }

    if (BuildConfig.DEBUG)
      Log.d(TAG, cmd.mkString(" "))

    kcptunProcess = new GuardedProcess(cmd).start()
  }

  def startShadowsocksUDPDaemon() {
    val conf = ConfigUtils
      .SHADOWSOCKS.formatLocal(Locale.ENGLISH, profile.host, profile.remotePort, profile.localPort,
        profile.password, profile.method, 600)
    Utils.printToFile(new File(getApplicationInfo.dataDir + "/ss-local-udp-vpn.conf"))(p => {
      p.println(conf)
    })

    val cmd = ArrayBuffer[String](getApplicationInfo.dataDir + "/ss-local", "-V", "-U"
      , "-b", "127.0.0.1"
      , "-t", "600"
      , "-P", getApplicationInfo.dataDir
      , "-c", getApplicationInfo.dataDir + "/ss-local-udp-vpn.conf")

    if (profile.auth) cmd += "-A"

    if (BuildConfig.DEBUG) Log.d(TAG, cmd.mkString(" "))

    sstunnelProcess = new GuardedProcess(cmd).start()
  }

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
    Utils.printToFile(new File(getApplicationInfo.dataDir + "/ss-local-vpn.conf"))(p => {
      p.println(conf)
    })

    val cmd = ArrayBuffer[String](getApplicationInfo.dataDir + "/ss-local", "-V"
      , "-b", "127.0.0.1"
      , "-t", "600"
      , "-P", getApplicationInfo.dataDir
      , "-c", getApplicationInfo.dataDir + "/ss-local-vpn.conf")

    if (profile.auth) cmd += "-A"

    if (profile.udpdns && !profile.kcp) cmd += "-u"

    if (profile.route != Route.ALL) {
      cmd += "--acl"
      cmd += getApplicationInfo.dataDir + '/' + profile.route + ".acl"
    }

    if (TcpFastOpen.sendEnabled) cmd += "--fast-open"

    if (BuildConfig.DEBUG) Log.d(TAG, cmd.mkString(" "))

    sslocalProcess = new GuardedProcess(cmd).start()
  }

  def startDnsTunnel() = {
    val conf = if (profile.kcp) {
      ConfigUtils
      .SHADOWSOCKS.formatLocal(Locale.ENGLISH, "127.0.0.1", profile.localPort + 90, profile.localPort + 63,
        profile.password, profile.method, 10)
    } else {
      ConfigUtils
      .SHADOWSOCKS.formatLocal(Locale.ENGLISH, profile.host, profile.remotePort, profile.localPort + 63,
        profile.password, profile.method, 10)
    }
    Utils.printToFile(new File(getApplicationInfo.dataDir + "/ss-tunnel-vpn.conf"))(p => {
      p.println(conf)
    })
    val cmd = ArrayBuffer[String](getApplicationInfo.dataDir + "/ss-tunnel"
      , "-V"
      , "-t", "10"
      , "-b", "127.0.0.1"
      , "-P", getApplicationInfo.dataDir
      , "-c", getApplicationInfo.dataDir + "/ss-tunnel-vpn.conf")

    cmd += "-L"
    if (profile.route == Route.CHINALIST)
      cmd += "114.114.114.114:53"
    else
      cmd += "8.8.8.8:53"

    if (profile.auth) cmd += "-A"

    if (BuildConfig.DEBUG) Log.d(TAG, cmd.mkString(" "))

    sstunnelProcess = new GuardedProcess(cmd).start()
  }

  def startDnsDaemon() {
    val reject = if (profile.ipv6) "224.0.0.0/3" else "224.0.0.0/3, ::/0"
    val protect = "protect = \"" + protectPath +"\";"
    val conf = profile.route match {
      case Route.BYPASS_CHN | Route.BYPASS_LAN_CHN | Route.GFWLIST => {
        ConfigUtils.PDNSD_DIRECT.formatLocal(Locale.ENGLISH, protect, getApplicationInfo.dataDir,
          "0.0.0.0", profile.localPort + 53, "114.114.114.114, 223.5.5.5, 1.2.4.8",
          getBlackList, reject, profile.localPort + 63, reject)
      }
      case Route.CHINALIST => {
        ConfigUtils.PDNSD_DIRECT.formatLocal(Locale.ENGLISH, protect, getApplicationInfo.dataDir,
          "0.0.0.0", profile.localPort + 53, "8.8.8.8, 8.8.4.4, 208.67.222.222",
          "", reject, profile.localPort + 63, reject)
      }
      case _ => {
        ConfigUtils.PDNSD_LOCAL.formatLocal(Locale.ENGLISH, protect, getApplicationInfo.dataDir,
          "0.0.0.0", profile.localPort + 53, profile.localPort + 63, reject)
      }
    }
    Utils.printToFile(new File(getApplicationInfo.dataDir + "/pdnsd-vpn.conf"))(p => {
      p.println(conf)
    })
    val cmd = Array(getApplicationInfo.dataDir + "/pdnsd", "-c", getApplicationInfo.dataDir + "/pdnsd-vpn.conf")

    if (BuildConfig.DEBUG) Log.d(TAG, cmd.mkString(" "))

    pdnsdProcess = new GuardedProcess(cmd).start()
  }

  @SuppressLint(Array("NewApi"))
  def startVpn(): Int = {

    val builder = new Builder()
    builder
      .setSession(profile.name)
      .setMtu(VPN_MTU)
      .addAddress(PRIVATE_VLAN.formatLocal(Locale.ENGLISH, "1"), 24)

    builder.addDnsServer("8.8.8.8")

    if (profile.ipv6) {
      builder.addAddress(PRIVATE_VLAN6.formatLocal(Locale.ENGLISH, "1"), 126)
      builder.addRoute("::", 0)
    }

    if (Utils.isLollipopOrAbove) {

      if (profile.proxyApps) {
        for (pkg <- profile.individual.split('\n')) {
          try {
            if (!profile.bypass) {
              builder.addAllowedApplication(pkg)
            } else {
              builder.addDisallowedApplication(pkg)
            }
          } catch {
            case ex: NameNotFoundException =>
              Log.e(TAG, "Invalid package name", ex)
          }
        }
      }
    }

    if (profile.route == Route.ALL || profile.route == Route.BYPASS_CHN) {
      builder.addRoute("0.0.0.0", 0)
    } else {
      val privateList = getResources.getStringArray(R.array.bypass_private_route)
      privateList.foreach(cidr => {
        val addr = cidr.split('/')
        builder.addRoute(addr(0), addr(1).toInt)
      })
    }

    builder.addRoute("8.8.0.0", 16)

    conn = builder.establish()
    if (conn == null) throw new NullConnectionException

    val fd = conn.getFd

    var cmd = ArrayBuffer[String](getApplicationInfo.dataDir + "/tun2socks",
      "--netif-ipaddr", PRIVATE_VLAN.formatLocal(Locale.ENGLISH, "2"),
      "--netif-netmask", "255.255.255.0",
      "--socks-server-addr", "127.0.0.1:" + profile.localPort,
      "--tunfd", fd.toString,
      "--tunmtu", VPN_MTU.toString,
      "--sock-path", getApplicationInfo.dataDir + "/sock_path",
      "--loglevel", "3")

    if (profile.ipv6)
      cmd += ("--netif-ip6addr", PRIVATE_VLAN6.formatLocal(Locale.ENGLISH, "2"))

    if (profile.udpdns)
      cmd += "--enable-udprelay"
    else
      cmd += ("--dnsgw", "%s:%d".formatLocal(Locale.ENGLISH, PRIVATE_VLAN.formatLocal(Locale.ENGLISH, "1"),
        profile.localPort + 53))

    if (BuildConfig.DEBUG) Log.d(TAG, cmd.mkString(" "))

    tun2socksProcess = new GuardedProcess(cmd).start(() => sendFd(fd))

    fd
  }

  def sendFd(fd: Int): Boolean = {
    if (fd != -1) {
      var tries = 1
      while (tries < 5) {
        Thread.sleep(1000 * tries)
        if (System.sendfd(fd, getApplicationInfo.dataDir + "/sock_path") != -1) {
          return true
        }
        tries += 1
      }
    }
    false
  }
}
