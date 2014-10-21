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
import java.net.InetAddress

import android.app._
import android.content._
import android.content.pm.{PackageInfo, PackageManager}
import android.net.VpnService
import android.os._
import android.support.v4.app.NotificationCompat
import android.util.Log
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

  def isACLEnabled: Boolean = {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
      true
    } else {
      false
    }
  }

  def isByass(net: SubnetUtils): Boolean = {
    val info = net.getInfo
    info.isInRange(config.proxy)
  }

  def startShadowsocksDaemon() {

    if (isACLEnabled && config.isGFWList) {
      val chn_list: Array[String] = getResources.getStringArray(R.array.chn_list_full)
      ConfigUtils.printToFile(new File(Path.BASE + "chn.acl"))(p => {
        chn_list.foreach(item => p.println(item))
      })
    }

    val cmd = new ArrayBuffer[String]
    cmd += ("ss-local" , "-u"
            , "-b" , "127.0.0.1"
            , "-s" , config.proxy
            , "-p" , config.remotePort.toString
            , "-l" , config.localPort.toString
            , "-k" , config.sitekey
            , "-m" , config.encMethod
            , "-f" , Path.BASE + "ss-local.pid")

    if (config.isGFWList && isACLEnabled) {
      cmd += "--acl"
      cmd += (Path.BASE + "chn.acl")
    }

    if (BuildConfig.DEBUG) Log.d(TAG, cmd.mkString(" "))
    Core.sslocal(cmd.toArray)
  }

  def startDnsTunnel() = {
    val cmd = new ArrayBuffer[String]
    cmd += ("ss-tunnel"
      , "-b" , "127.0.0.1"
      , "-l" , "8163"
      , "-L" , "8.8.8.8:53"
      , "-s" , config.proxy
      , "-p" , config.remotePort.toString
      , "-k" , config.sitekey
      , "-m" , config.encMethod
      , "-f" , Path.BASE + "ss-tunnel.pid")
    if (BuildConfig.DEBUG) Log.d(TAG, cmd.mkString(" "))
    Core.sstunnel(cmd.toArray)
  }

  def startDnsDaemon() {
    val conf = {
      ConfigUtils.PDNSD_LOCAL.format("0.0.0.0", 8163)
    }
    ConfigUtils.printToFile(new File(Path.BASE + "pdnsd.conf"))(p => {
      p.println(conf)
    })
    val cmd = Path.BASE + "pdnsd -c " + Path.BASE + "pdnsd.conf"
    if (BuildConfig.DEBUG) Log.d(TAG, cmd)
    Core.pdnsd(cmd.split(" "))
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
      .addAddress(PRIVATE_VLAN.format("1"), 24)
      .addDnsServer("8.8.8.8")

    if (Build.VERSION.SDK_INT == Build.VERSION_CODES.LOLLIPOP) {
      if (!config.isGlobalProxy) {
        val apps = AppManager.getProxiedApps(this, config.proxiedAppString)
        val pkgSet: mutable.HashSet[String] = new mutable.HashSet[String]
        for (app <- apps) {
          if (app.proxied) {
            pkgSet.add(app.name)
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
    } else if (!isACLEnabled && config.isGFWList) {
      val gfwList = {
        if (Build.VERSION.SDK_INT == Build.VERSION_CODES.KITKAT) {
          getResources.getStringArray(R.array.simple_list)
        } else {
          getResources.getStringArray(R.array.gfw_list)
        }
      }
      gfwList.foreach(cidr => {
        val net = new SubnetUtils(cidr)
        if (!isByass(net)) {
          val addr = cidr.split('/')
          builder.addRoute(addr(0), addr(1).toInt)
        }
      })
    } else {
      if (isACLEnabled) {
        builder.addRoute("0.0.0.0", 0)
      } else {
        for (i <- 1 to 223) {
          if (i != 26 && i != 127) {
            val addr = i.toString + ".0.0.0"
            val cidr = addr + "/8"
            val net = new SubnetUtils(cidr)

            if (!isByass(net)) {
              if (!InetAddress.getByName(addr).isSiteLocalAddress) {
                builder.addRoute(addr, 8)
              }
            } else {
              for (j <- 0 to 255) {
                val subAddr = i.toString + "." + j.toString + ".0.0"
                val subCidr = subAddr + "/16"
                val subNet = new SubnetUtils(subCidr)
                if (!isByass(subNet)) {
                  if (!InetAddress.getByName(subAddr).isSiteLocalAddress) {
                    builder.addRoute(subAddr, 16)
                  }
                }
              }
            }
          }
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
      + "--pid %stun2socks.pid")
      .format(PRIVATE_VLAN.format("2"), config.localPort, fd, VPN_MTU, Path.BASE)

    if (config.isUdpDns)
      cmd += " --enable-udprelay"
    else
      cmd += " --dnsgw %s:8153".format(PRIVATE_VLAN.format("1"))

    if (BuildConfig.DEBUG) Log.d(TAG, cmd)

    Core.tun2socks(cmd.split(" "))
  }

  /** Called when the activity is first created. */
  def handleConnection: Boolean = {
    startVpn()
    startShadowsocksDaemon()
    if (!config.isUdpDns) {
      startDnsDaemon()
      startDnsTunnel()
    }
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
    for (task <- Array("ss-local", "ss-tunnel", "tun2socks", "pdnsd")) {
      try {
        val pid = scala.io.Source.fromFile(Path.BASE + task + ".pid").mkString.trim.toInt
        Process.killProcess(pid)
        Log.d(TAG, "kill pid: " + pid)
      } catch {
        case e: Throwable => Log.e(TAG, "unable to kill " + task, e)
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
    changeState(State.STOPPED)

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
    if (callbackCount == 0) {
      stopSelf()
    }

    // clean up the context
    if (receiver != null) {
      unregisterReceiver(receiver)
      receiver = null
    }
  }

  override def stopBackgroundService() {
    stopRunner()
  }

  override def getTag = TAG
  override def getServiceMode = Mode.VPN
  override def getContext = getBaseContext
}
