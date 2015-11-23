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

import android.app._
import android.content._
import android.content.pm.{PackageInfo, PackageManager}
import android.net.VpnService
import android.os._
import android.support.v4.app.NotificationCompat
import android.support.v4.content.ContextCompat
import android.util.Log
import android.widget.Toast
import com.github.shadowsocks.aidl.Config
import com.github.shadowsocks.utils._
import org.apache.commons.net.util.SubnetUtils

import scala.collection.JavaConversions._
import scala.collection.mutable
import scala.collection.mutable.ArrayBuffer
import scala.concurrent.ExecutionContext.Implicits.global
import scala.concurrent.Future

class ShadowsocksVpnService extends VpnService with BaseService {
  val TAG = "ShadowsocksVpnService"
  val VPN_MTU = 1500
  val PRIVATE_VLAN = "26.26.26.%s"
  val PRIVATE_VLAN6 = "fdfe:dcba:9876::%s"
  var conn: ParcelFileDescriptor = null
  var apps: Array[ProxiedApp] = null
  var vpnThread: ShadowsocksVpnThread = null
  var closeReceiver: BroadcastReceiver = null

  var sslocalProcess: Process = null
  var sstunnelProcess: Process = null
  var pdnsdProcess: Process = null
  var tun2socksProcess: Process = null

  def isByass(net: SubnetUtils): Boolean = {
    val info = net.getInfo
    info.isInRange(config.proxy)
  }

  def isPrivateA(a: Int): Boolean = {
    if (a == 10 || a == 192 || a == 172) {
      true
    } else {
      false
    }
  }

  def isPrivateB(a: Int, b: Int): Boolean = {
    if (a == 10 || (a == 192 && b == 168) || (a == 172 && b >= 16 && b < 32)) {
      true
    } else {
      false
    }
  }

  override def onBind(intent: Intent): IBinder = {
    val action = intent.getAction
    if (VpnService.SERVICE_INTERFACE == action) {
      return super.onBind(intent)
    } else if (Action.SERVICE == action) {
      return binder
    }
    null
  }

  def notifyForegroundAlert(title: String, info: String, visible: Boolean) {
    val openIntent = new Intent(this, classOf[Shadowsocks])
    val contentIntent = PendingIntent.getActivity(this, 0, openIntent, 0)
    val closeIntent = new Intent(Action.CLOSE)
    val actionIntent = PendingIntent.getBroadcast(this, 0, closeIntent, 0)
    val builder = new NotificationCompat.Builder(this)

    builder
      .setWhen(0)
      .setColor(ContextCompat.getColor(this, R.color.material_accent_500))
      .setTicker(title)
      .setContentTitle(getString(R.string.app_name))
      .setContentText(info)
      .setContentIntent(contentIntent)
      .setSmallIcon(R.drawable.ic_stat_shadowsocks)
      .addAction(android.R.drawable.ic_menu_close_clear_cancel, getString(R.string.stop),
        actionIntent)

    if (visible)
      builder.setPriority(NotificationCompat.PRIORITY_DEFAULT)
    else
      builder.setPriority(NotificationCompat.PRIORITY_MIN)

    startForeground(1, builder.build)
  }

  override def onCreate() {
    super.onCreate()
    ConfigUtils.refresh(this)
  }

  override def onRevoke() {
    stopRunner()
  }

  override def stopRunner() {

    super.stopRunner()

    if (vpnThread != null) {
      vpnThread.stopThread()
      vpnThread = null
    }

    stopForeground(true)

    // channge the state
    changeState(State.STOPPING)

    ShadowsocksApplication.track(TAG, "stop")

    // reset VPN
    killProcesses()

    // close connections
    if (conn != null) {
      conn.close()
      conn = null
    }

    // stop the service if no callback registered
    if (getCallbackCount == 0) {
      stopSelf()
    }

    // clean up recevier
    if (closeReceiver != null) {
      unregisterReceiver(closeReceiver)
      closeReceiver = null
    }

    // channge the state
    changeState(State.STOPPED)
  }

  def getVersionName: String = {
    var version: String = null
    try {
      val pi: PackageInfo = getPackageManager.getPackageInfo(getPackageName, 0)
      version = pi.versionName
    } catch {
      case e: PackageManager.NameNotFoundException =>
        version = "Package name not found"
    }
    version
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

  override def startRunner(config: Config) {

    super.startRunner(config)

    vpnThread = new ShadowsocksVpnThread(this)
    vpnThread.start()

    // register close receiver
    val filter = new IntentFilter()
    filter.addAction(Intent.ACTION_SHUTDOWN)
    filter.addAction(Action.CLOSE)
    closeReceiver = (context: Context, intent: Intent) => {
      Toast.makeText(context, R.string.stopping, Toast.LENGTH_SHORT).show()
      stopRunner()
    }
    registerReceiver(closeReceiver, filter)

    // ensure the VPNService is prepared
    if (VpnService.prepare(this) != null) {
      val i = new Intent(this, classOf[ShadowsocksRunnerActivity])
      i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
      startActivity(i)
      return
    }

    ShadowsocksApplication.track(TAG, "start")

    changeState(State.CONNECTING)

    Future {
      if (config.proxy == "198.199.101.152") {
        val holder = ShadowsocksApplication.containerHolder
        try {
          this.config = ConfigUtils.getPublicConfig(getBaseContext, holder.getContainer, config)
        } catch {
          case ex: Exception =>
            changeState(State.STOPPED, getString(R.string.service_failed))
            stopRunner()
            this.config = null
        }
      }

      if (config != null) {

        // reset the context
        killProcesses()

        // Resolve the server address
        var resolved: Boolean = false
        if (!Utils.isNumeric(config.proxy)) {
          Utils.resolve(config.proxy, enableIPv6 = true) match {
            case Some(addr) =>
              config.proxy = addr
              resolved = true
            case None => resolved = false
          }
        } else {
          resolved = true
        }

        if (resolved && handleConnection) {
          notifyForegroundAlert(getString(R.string.forward_success),
            getString(R.string.service_running).formatLocal(Locale.ENGLISH, config.profileName), false)
          changeState(State.CONNECTED)
        } else {
          changeState(State.STOPPED, getString(R.string.service_failed))
          stopRunner()
        }
      }
    }
  }

  /** Called when the activity is first created. */
  def handleConnection: Boolean = {
    startShadowsocksDaemon()
    if (!config.isUdpDns) {
      startDnsDaemon()
      startDnsTunnel()
    }

    val fd = startVpn()

    if (fd != -1) {
      var tries = 1
      while (tries < 5) {
        Thread.sleep(1000 * tries)
        if (System.sendfd(fd) != -1) {
          return true
        }
        tries += 1
      }
    }
    false
  }

  def startShadowsocksDaemon() {

    if (config.route != Route.ALL) {
      val acl: Array[String] = config.route match {
        case Route.BYPASS_LAN => getResources.getStringArray(R.array.private_route)
        case Route.BYPASS_CHN => getResources.getStringArray(R.array.chn_route_full)
      }
      ConfigUtils.printToFile(new File(Path.BASE + "acl.list"))(p => {
        acl.foreach(item => p.println(item))
      })
    }

    val conf = ConfigUtils
      .SHADOWSOCKS.formatLocal(Locale.ENGLISH, config.proxy, config.remotePort, config.localPort,
        config.sitekey, config.encMethod, 600)
    ConfigUtils.printToFile(new File(Path.BASE + "ss-local-vpn.conf"))(p => {
      p.println(conf)
    })

    val cmd = new ArrayBuffer[String]
    cmd += (Path.BASE + "ss-local", "-V", "-u"
      , "-b", "127.0.0.1"
      , "-t", "600"
      , "-c", Path.BASE + "ss-local-vpn.conf")

    if (config.isAuth) cmd += "-A"

    if (config.route != Route.ALL) {
      cmd += "--acl"
      cmd += (Path.BASE + "acl.list")
    }

    if (BuildConfig.DEBUG) Log.d(TAG, cmd.mkString(" "))

    sslocalProcess = new ProcessBuilder()
      .command(cmd)
      .redirectErrorStream(true)
      .start()
  }

  def startDnsTunnel() = {
    val conf = ConfigUtils
      .SHADOWSOCKS.formatLocal(Locale.ENGLISH, config.proxy, config.remotePort, 8163,
        config.sitekey, config.encMethod, 10)
    ConfigUtils.printToFile(new File(Path.BASE + "ss-tunnel-vpn.conf"))(p => {
      p.println(conf)
    })
    val cmd = new ArrayBuffer[String]
    cmd += (Path.BASE + "ss-tunnel"
      , "-V"
      , "-u"
      , "-t", "10"
      , "-b", "127.0.0.1"
      , "-l", "8163"
      , "-L", "8.8.8.8:53"
      , "-c", Path.BASE + "ss-tunnel-vpn.conf")

    if (config.isAuth) cmd += "-A"

    if (BuildConfig.DEBUG) Log.d(TAG, cmd.mkString(" "))

    sstunnelProcess = new ProcessBuilder()
      .command(cmd)
      .redirectErrorStream(true)
      .start()
  }

  def startDnsDaemon() {
    val ipv6 = if (config.isIpv6) "" else "reject = ::/0;"
    val conf = {
      if (config.route == Route.BYPASS_CHN) {
        val reject = ConfigUtils.getRejectList(getContext)
        val blackList = ConfigUtils.getBlackList(getContext)
        ConfigUtils.PDNSD_DIRECT.formatLocal(Locale.ENGLISH, "0.0.0.0", 8153,
          reject, blackList, 8163, ipv6)
      } else {
        ConfigUtils.PDNSD_LOCAL.formatLocal(Locale.ENGLISH, "0.0.0.0", 8153,
          8163, ipv6)
      }
    }
    ConfigUtils.printToFile(new File(Path.BASE + "pdnsd-vpn.conf"))(p => {
      p.println(conf)
    })
    val cmd = Path.BASE + "pdnsd -c " + Path.BASE + "pdnsd-vpn.conf"

    if (BuildConfig.DEBUG) Log.d(TAG, cmd)

    pdnsdProcess = new ProcessBuilder()
      .command(cmd.split(" ").toSeq)
      .redirectErrorStream(true)
      .start()
  }

  override def getContext = getBaseContext

  def startVpn(): Int = {

    val builder = new Builder()
    builder
      .setSession(config.profileName)
      .setMtu(VPN_MTU)
      .addAddress(PRIVATE_VLAN.formatLocal(Locale.ENGLISH, "1"), 24)
      .addDnsServer("8.8.8.8")

    if (config.isIpv6) {
      builder.addAddress(PRIVATE_VLAN6.formatLocal(Locale.ENGLISH, "1"), 126)
      builder.addRoute("::", 0)
    }

    if (Utils.isLollipopOrAbove) {

      if (config.isProxyApps) {
        val apps = AppManager.getProxiedApps(this, config.proxiedAppString)
        val pkgSet: mutable.HashSet[String] = new mutable.HashSet[String]
        for (app <- apps) {
          if (app.proxied) {
            pkgSet.add(app.packageName)
          }
        }
        for (pkg <- pkgSet) {
          if (!config.isBypassApps) {
            builder.addAllowedApplication(pkg)
          } else {
            builder.addDisallowedApplication(pkg)
          }
        }
      }
    }

    if (config.route == Route.ALL) {
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
      stopRunner()
      return -1
    }

    val fd = conn.getFd

    var cmd = (Path.BASE +
      "tun2socks --netif-ipaddr %s "
      + "--netif-netmask 255.255.255.0 "
      + "--socks-server-addr 127.0.0.1:%d "
      + "--tunfd %d "
      + "--tunmtu %d "
      + "--loglevel 3")
      .formatLocal(Locale.ENGLISH,
        PRIVATE_VLAN.formatLocal(Locale.ENGLISH, "2"),
        config.localPort, fd, VPN_MTU, Path.BASE)

    if (config.isIpv6)
      cmd += " --netif-ip6addr " + PRIVATE_VLAN6.formatLocal(Locale.ENGLISH, "2")

    if (config.isUdpDns)
      cmd += " --enable-udprelay"
    else
      cmd += " --dnsgw %s:8153".formatLocal(Locale.ENGLISH, PRIVATE_VLAN.formatLocal(Locale.ENGLISH, "1"))

    if (BuildConfig.DEBUG) Log.d(TAG, cmd)

    tun2socksProcess = new ProcessBuilder()
      .command(cmd.split(" ").toSeq)
      .redirectErrorStream(true)
      .start()

    fd
  }

  override def stopBackgroundService() {
    stopSelf()
  }

  override def getTag = TAG

  override def getServiceMode = Mode.VPN
}
