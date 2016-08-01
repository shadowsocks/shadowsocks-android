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
import java.lang.Process
import java.util.Locale

import android.content._
import android.content.pm.PackageManager.NameNotFoundException
import android.net.VpnService
import android.os._
import android.util.Log
import com.github.shadowsocks.utils._
import com.github.shadowsocks.ShadowsocksApplication.app
import com.github.shadowsocks.database.Profile

import scala.collection.mutable.ArrayBuffer

class ShadowsocksVpnService extends VpnService with BaseService {
  val TAG = "ShadowsocksVpnService"
  val VPN_MTU = 1500
  val PRIVATE_VLAN = "26.26.26.%s"
  val PRIVATE_VLAN6 = "fdfe:dcba:9876::%s"
  var conn: ParcelFileDescriptor = _
  var vpnThread: ShadowsocksVpnThread = _
  private var notification: ShadowsocksNotification = _

  var sslocalProcess: Process = _
  var sstunnelProcess: Process = _
  var pdnsdProcess: Process = _
  var tun2socksProcess: Process = _

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

    if (profile != null) {

      vpnThread = new ShadowsocksVpnThread(this)
      vpnThread.start()

      // reset the context
      killProcesses()

      // Resolve the server address
      var resolved: Boolean = false
      if (!Utils.isNumeric(profile.host)) {
        Utils.resolve(profile.host, enableIPv6 = true) match {
          case Some(addr) =>
            profile.host = addr
            resolved = true
          case None => resolved = false
        }
      } else {
        resolved = true
      }

      if (!resolved) stopRunner(true, getString(R.string.invalid_server)) else if (handleConnection) {
        changeState(State.CONNECTED)
        notification = new ShadowsocksNotification(this, profile.name)
      } else {
        changeState(State.STOPPED, getString(R.string.service_failed))
        stopRunner(true)
      }
    }
  }

  /** Called when the activity is first created. */
  def handleConnection: Boolean = {
    startShadowsocksDaemon()
    if (!profile.udpdns) {
      startDnsDaemon()
      startDnsTunnel()
    }

    val fd = startVpn()
    sendFd(fd)
  }

  def startShadowsocksDaemon() {

    if (profile.route != Route.ALL) {
      val acl: Array[Array[String]] = profile.route match {
        case Route.BYPASS_LAN => Array(getResources.getStringArray(R.array.private_route))
        case Route.BYPASS_CHN => Array(getResources.getStringArray(R.array.chn_route))
        case Route.BYPASS_LAN_CHN =>
          Array(getResources.getStringArray(R.array.private_route), getResources.getStringArray(R.array.chn_route))
      }
      Utils.printToFile(new File(getApplicationInfo.dataDir + "/acl.list"))(p => {
        acl.flatten.foreach(p.println)
      })
    }

    val conf = ConfigUtils
      .SHADOWSOCKS.formatLocal(Locale.ENGLISH, profile.host, profile.remotePort, profile.localPort,
        profile.password, profile.method, 600, profile.protocol, profile.obfs, profile.obfs_param)
    Utils.printToFile(new File(getApplicationInfo.dataDir + "/ss-local-vpn.conf"))(p => {
      p.println(conf)
    })

    val cmd = new ArrayBuffer[String]
    cmd += (getApplicationInfo.dataDir + "/ss-local", "-V", "-u"
      , "-b", "127.0.0.1"
      , "-t", "600"
      , "-P", getApplicationInfo.dataDir
      , "-c", getApplicationInfo.dataDir + "/ss-local-vpn.conf")

    if (profile.auth) cmd += "-A"

    if (profile.route != Route.ALL) {
      cmd += "--acl"
      cmd += (getApplicationInfo.dataDir + "/acl.list")
    }

    if (BuildConfig.DEBUG) Log.d(TAG, cmd.mkString(" "))

    sslocalProcess = new GuardedProcess(cmd).start()
  }

  def startDnsTunnel() = {
    val conf = ConfigUtils
      .SHADOWSOCKS.formatLocal(Locale.ENGLISH, profile.host, profile.remotePort, 8163,
        profile.password, profile.method, 10, profile.protocol, profile.obfs, profile.obfs_param)
    Utils.printToFile(new File(getApplicationInfo.dataDir + "/ss-tunnel-vpn.conf"))(p => {
      p.println(conf)
    })
    val cmd = new ArrayBuffer[String]
    cmd += (getApplicationInfo.dataDir + "/ss-tunnel"
      , "-V"
      , "-u"
      , "-t", "10"
      , "-b", "127.0.0.1"
      , "-l", "8163"
      , "-L", "8.8.8.8:53"
      , "-P", getApplicationInfo.dataDir
      , "-c", getApplicationInfo.dataDir + "/ss-tunnel-vpn.conf")

    if (profile.auth) cmd += "-A"

    if (BuildConfig.DEBUG) Log.d(TAG, cmd.mkString(" "))

    sstunnelProcess = new GuardedProcess(cmd).start()
  }

  def startDnsDaemon() {
    val ipv6 = if (profile.ipv6) "" else "reject = ::/0;"
    val conf = {
      if (profile.route == Route.BYPASS_CHN || profile.route == Route.BYPASS_LAN_CHN) {
        ConfigUtils.PDNSD_DIRECT.formatLocal(Locale.ENGLISH, getApplicationInfo.dataDir,
          "0.0.0.0", 8153, getBlackList, 8163, ipv6)
      } else {
        ConfigUtils.PDNSD_LOCAL.formatLocal(Locale.ENGLISH, getApplicationInfo.dataDir,
          "0.0.0.0", 8153, 8163, ipv6)
      }
    }
    Utils.printToFile(new File(getApplicationInfo.dataDir + "/pdnsd-vpn.conf"))(p => {
      p.println(conf)
    })
    val cmd = getApplicationInfo.dataDir + "/pdnsd -c " + getApplicationInfo.dataDir + "/pdnsd-vpn.conf"

    if (BuildConfig.DEBUG) Log.d(TAG, cmd)

    pdnsdProcess = new GuardedProcess(cmd.split(" ").toSeq).start()
  }

  def startVpn(): Int = {

    val builder = new Builder()
    builder
      .setSession(profile.name)
      .setMtu(VPN_MTU)
      .addAddress(PRIVATE_VLAN.formatLocal(Locale.ENGLISH, "1"), 24)
      .addDnsServer("8.8.8.8")

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
              Log.e(TAG, "Invalid package name", ex);
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

    try {
      conn = builder.establish()
      if (conn == null) changeState(State.STOPPED, getString(R.string.reboot_required))
    } catch {
      case ex: IllegalStateException =>
        changeState(State.STOPPED, ex.getMessage)
        conn = null
      case ex: Exception =>
        ex.printStackTrace()
        conn = null
    }

    if (conn == null) {
      stopRunner(true)
      return -1
    }

    val fd = conn.getFd

    var cmd = (getApplicationInfo.dataDir +
      "/tun2socks --netif-ipaddr %s "
      + "--netif-netmask 255.255.255.0 "
      + "--socks-server-addr 127.0.0.1:%d "
      + "--tunfd %d "
      + "--tunmtu %d "
      + "--sock-path %s "
      + "--loglevel 3")
      .formatLocal(Locale.ENGLISH,
        PRIVATE_VLAN.formatLocal(Locale.ENGLISH, "2"),
        profile.localPort, fd, VPN_MTU, getApplicationInfo.dataDir + "/sock_path")

    if (profile.ipv6)
      cmd += " --netif-ip6addr " + PRIVATE_VLAN6.formatLocal(Locale.ENGLISH, "2")

    if (profile.udpdns)
      cmd += " --enable-udprelay"
    else
      cmd += " --dnsgw %s:8153".formatLocal(Locale.ENGLISH, PRIVATE_VLAN.formatLocal(Locale.ENGLISH, "1"))

    if (BuildConfig.DEBUG) Log.d(TAG, cmd)

    tun2socksProcess = new GuardedProcess(cmd.split(" ").toSeq).start(() => sendFd(fd))

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
