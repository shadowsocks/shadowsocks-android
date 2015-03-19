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

import android.app._
import android.content._
import android.content.pm.{PackageInfo, PackageManager}
import android.net.VpnService
import android.os._
import android.util.Log
import android.widget.Toast
import com.github.shadowsocks.aidl.Config
import com.github.shadowsocks.utils._
import com.google.android.gms.analytics.HitBuilders
import org.apache.commons.net.util.SubnetUtils
import org.apache.http.conn.util.InetAddressUtils

import scala.collection.mutable
import scala.collection.mutable.ArrayBuffer
import scala.concurrent.ops._

class ShadowsocksVpnService extends VpnService with BaseService {

  val TAG = "ShadowsocksVpnService"

  val VPN_MTU = 1500
  val PRIVATE_VLAN = "26.26.26.%s"

  var conn: ParcelFileDescriptor = null
  var notificationManager: NotificationManager = null
  var receiver: BroadcastReceiver = null
  var apps: Array[ProxiedApp] = null
  var config: Config = null

  private lazy val application = getApplication.asInstanceOf[ShadowsocksApplication]

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

  def startShadowsocksDaemon() {

    if (Utils.isLollipopOrAbove && config.route != Route.ALL) {
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
        config.sitekey, config.encMethod, 10)
    ConfigUtils.printToFile(new File(Path.BASE + "ss-local-vpn.conf"))(p => {
      p.println(conf)
    })

    val cmd = new ArrayBuffer[String]
    cmd +=(Path.BASE + "ss-local", "-u"
      , "-b" , "127.0.0.1"
      , "-t" , "600"
      , "-c" , Path.BASE + "ss-local-vpn.conf"
      , "-f" , Path.BASE + "ss-local-vpn.pid")

    if (Utils.isLollipopOrAbove && config.route != Route.ALL) {
      cmd += "--acl"
      cmd += (Path.BASE + "acl.list")
    }

    if (BuildConfig.DEBUG) Log.d(TAG, cmd.mkString(" "))
    Console.runCommand(cmd.mkString(" "))
  }

  def startDnsTunnel() = {
    val conf = ConfigUtils
      .SHADOWSOCKS.formatLocal(Locale.ENGLISH, config.proxy, config.remotePort, 8163,
        config.sitekey, config.encMethod, 10)
    ConfigUtils.printToFile(new File(Path.BASE + "ss-tunnel-vpn.conf"))(p => {
      p.println(conf)
    })
    val cmd = new ArrayBuffer[String]
    cmd +=(Path.BASE + "ss-tunnel"
      , "-u"
      , "-t" , "10"
      , "-b" , "127.0.0.1"
      , "-l" , "8163"
      , "-L" , "8.8.8.8:53"
      , "-c" , Path.BASE + "ss-tunnel-vpn.conf"
      , "-f" , Path.BASE + "ss-tunnel-vpn.pid")

    if (BuildConfig.DEBUG) Log.d(TAG, cmd.mkString(" "))
    Console.runCommand(cmd.mkString(" "))
  }

  def startDnsDaemon() {
    val conf = {
      if (config.route == Route.BYPASS_CHN) {
        val reject = ConfigUtils.getRejectList(getContext, application)
        val blackList = ConfigUtils.getBlackList(getContext, application)
        ConfigUtils.PDNSD_DIRECT.formatLocal(Locale.ENGLISH, "0.0.0.0", 8153,
          Path.BASE + "pdnsd-vpn.pid", reject, blackList, 8163)
      } else {
        ConfigUtils.PDNSD_LOCAL.formatLocal(Locale.ENGLISH, "0.0.0.0", 8153,
          Path.BASE + "pdnsd-vpn.pid", 8163)
      }
    }
    ConfigUtils.printToFile(new File(Path.BASE + "pdnsd-vpn.conf"))(p => {
      p.println(conf)
    })
    val cmd = Path.BASE + "pdnsd -c " + Path.BASE + "pdnsd-vpn.conf"

    if (BuildConfig.DEBUG) Log.d(TAG, cmd)
    Console.runCommand(cmd)
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

  def startVpn() {

    val builder = new Builder()
    builder
      .setSession(config.profileName)
      .setMtu(VPN_MTU)
      .addAddress(PRIVATE_VLAN.formatLocal(Locale.ENGLISH, "1"), 24)
      .addDnsServer("8.8.8.8")

    if (Utils.isLollipopOrAbove) {

      builder.allowFamily(android.system.OsConstants.AF_INET6)

      if (!config.isGlobalProxy) {
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

        if (config.isBypassApps) {
          builder.addDisallowedApplication(this.getPackageName)
        }
      } else {
        builder.addDisallowedApplication(this.getPackageName)
      }
    }

    if (InetAddressUtils.isIPv6Address(config.proxy)) {
      builder.addRoute("0.0.0.0", 0)
    } else {
      if (!Utils.isLollipopOrAbove) {
        config.route match {
          case Route.BYPASS_LAN =>
            for (i <- 1 to 223) {
              if (i != 26 && i != 127) {
                val addr = i.toString + ".0.0.0"
                val cidr = addr + "/8"
                val net = new SubnetUtils(cidr)

                if (!isByass(net) && !isPrivateA(i)) {
                  builder.addRoute(addr, 8)
                } else {
                  for (j <- 0 to 255) {
                    val subAddr = i.toString + "." + j.toString + ".0.0"
                    val subCidr = subAddr + "/16"
                    val subNet = new SubnetUtils(subCidr)
                    if (!isByass(subNet) && !isPrivateB(i, j)) {
                      builder.addRoute(subAddr, 16)
                    }
                  }
                }
              }
            }
          case Route.BYPASS_CHN =>
            val list = {
              if (Build.VERSION.SDK_INT == Build.VERSION_CODES.KITKAT) {
                getResources.getStringArray(R.array.simple_route)
              } else {
                getResources.getStringArray(R.array.gfw_route)
              }
            }
            list.foreach(cidr => {
              val net = new SubnetUtils(cidr)
              if (!isByass(net)) {
                val addr = cidr.split('/')
                builder.addRoute(addr(0), addr(1).toInt)
              }
            })
          case Route.ALL =>
            for (i <- 1 to 223) {
              if (i != 26 && i != 127) {
                val addr = i.toString + ".0.0.0"
                val cidr = addr + "/8"
                val net = new SubnetUtils(cidr)

                if (!isByass(net)) {
                  builder.addRoute(addr, 8)
                } else {
                  for (j <- 0 to 255) {
                    val subAddr = i.toString + "." + j.toString + ".0.0"
                    val subCidr = subAddr + "/16"
                    val subNet = new SubnetUtils(subCidr)
                    if (!isByass(subNet)) {
                      builder.addRoute(subAddr, 16)
                    }
                  }
                }
              }
            }
        }
      } else {
        if (config.route == Route.ALL) {
          builder.addRoute("0.0.0.0", 0)
        } else {
          val privateList = getResources.getStringArray(R.array.bypass_private_route)
          privateList.foreach(cidr => {
            val addr = cidr.split('/')
            builder.addRoute(addr(0), addr(1).toInt)
          })
        }
      }
    }

    builder.addRoute("8.8.0.0", 16)

    try {
      conn = builder.establish()
    } catch {
      case ex: IllegalStateException =>
        changeState(State.STOPPED, ex.getMessage)
        conn = null
      case ex: Exception => conn = null
    }

    if (conn == null) {
      stopRunner()
      return
    }

    val fd = conn.getFd

    var cmd = (Path.BASE +
      "tun2socks --netif-ipaddr %s "
      + "--netif-netmask 255.255.255.0 "
      + "--socks-server-addr 127.0.0.1:%d "
      + "--tunfd %d "
      + "--tunmtu %d "
      + "--loglevel 3 "
      + "--pid %stun2socks-vpn.pid")
      .formatLocal(Locale.ENGLISH, PRIVATE_VLAN.formatLocal(Locale.ENGLISH, "2"), config.localPort, fd, VPN_MTU, Path.BASE)

    if (config.isUdpDns)
      cmd += " --enable-udprelay"
    else
      cmd += " --dnsgw %s:8153".formatLocal(Locale.ENGLISH, PRIVATE_VLAN.formatLocal(Locale.ENGLISH, "1"))

    if (Utils.isLollipopOrAbove) {
      cmd += " --fake-proc"
    }

    if (BuildConfig.DEBUG) Log.d(TAG, cmd)

    System.exec(cmd)
  }

  /** Called when the activity is first created. */
  def handleConnection: Boolean = {
    startShadowsocksDaemon()
    if (!config.isUdpDns) {
      startDnsDaemon()
      startDnsTunnel()
    }
    startVpn()
    true
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

  override def onCreate() {
    super.onCreate()

    ConfigUtils.refresh(this)

    notificationManager = getSystemService(Context.NOTIFICATION_SERVICE)
      .asInstanceOf[NotificationManager]
  }

  override def onRevoke() {
    stopRunner()
  }

  def killProcesses() {
    for (task <- Array("ss-local", "ss-tunnel", "pdnsd", "tun2socks")) {
      try {
        val pid = scala.io.Source.fromFile(Path.BASE + task + "-vpn.pid").mkString.trim.toInt
        Process.killProcess(pid)
      } catch {
        case e: Throwable => Log.e(TAG, "unable to kill " + task)
      }
    }
  }

  override def startRunner(c: Config) {

    config = c

    // ensure the VPNService is prepared
    if (VpnService.prepare(this) != null) {
      val i = new Intent(this, classOf[ShadowsocksRunnerActivity])
      i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
      startActivity(i)
      return
    }

    // send event
    application.tracker.send(new HitBuilders.EventBuilder()
      .setCategory(TAG)
      .setAction("start")
      .setLabel(getVersionName)
      .build())

    // register close receiver
    val filter = new IntentFilter()
    filter.addAction(Intent.ACTION_SHUTDOWN)
    receiver = new BroadcastReceiver {
      def onReceive(p1: Context, p2: Intent) {
        Toast.makeText(p1, R.string.stopping, Toast.LENGTH_SHORT)
        stopRunner()
      }
    }
    registerReceiver(receiver, filter)

    changeState(State.CONNECTING)

    spawn {
      if (config.proxy == "198.199.101.152") {
        val holder = getApplication.asInstanceOf[ShadowsocksApplication].containerHolder
        try {
          config = ConfigUtils.getPublicConfig(getBaseContext, holder.getContainer, config)
        } catch {
          case ex: Exception =>
            changeState(State.STOPPED, getString(R.string.service_failed))
            stopRunner()
            config = null
        }
      }

      if (config != null) {

        // reset the context
        killProcesses()

        // Resolve the server address
        var resolved: Boolean = false
        if (!InetAddressUtils.isIPv4Address(config.proxy) &&
          !InetAddressUtils.isIPv6Address(config.proxy)) {
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
          changeState(State.CONNECTED)
        } else {
          changeState(State.STOPPED, getString(R.string.service_failed))
          stopRunner()
        }
      }
    }
  }

  override def stopRunner() {

    // channge the state
    changeState(State.STOPPING)

    // send event
    application.tracker.send(new HitBuilders.EventBuilder()
      .setCategory(TAG)
      .setAction("stop")
      .setLabel(getVersionName)
      .build())

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

    // clean up the context
    if (receiver != null) {
      unregisterReceiver(receiver)
      receiver = null
    }

    // channge the state
    changeState(State.STOPPED)
  }

  override def stopBackgroundService() {
    stopSelf()
  }

  override def getTag = TAG

  override def getServiceMode = Mode.VPN

  override def getContext = getBaseContext
}
