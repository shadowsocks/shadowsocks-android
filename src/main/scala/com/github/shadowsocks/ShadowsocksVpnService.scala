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

import android.app._
import android.content._
import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import android.os._
import android.support.v4.app.NotificationCompat
import android.util.Log
import com.google.analytics.tracking.android.{Fields, MapBuilder, EasyTracker}
import java.io._
import android.net.VpnService
import org.apache.http.conn.util.InetAddressUtils
import android.os.Message
import scala.concurrent.ops._
import org.apache.commons.net.util.SubnetUtils
import java.net.InetAddress
import com.github.shadowsocks.utils._
import scala.Some
import com.github.shadowsocks.aidl.{IShadowsocksService, Config}

class ShadowsocksVpnService extends VpnService with BaseService {

  val TAG = "ShadowsocksVpnService"

  val VPN_MTU = 1500

  val PRIVATE_VLAN = "26.26.26.%s"

  var conn: ParcelFileDescriptor = null
  var notificationManager: NotificationManager = null
  var receiver: BroadcastReceiver = null
  var apps: Array[ProxiedApp] = null
  var config: Config = null

  val handler: Handler = new Handler()

  def startShadowsocksDaemon() {
    val cmd: String = (Path.BASE +
      "shadowsocks -b 127.0.0.1 -s \"%s\" -p \"%d\" -l \"%d\" -k \"%s\" -m \"%s\" -f " +
      Path.BASE + "shadowsocks.pid")
      .format(config.proxy, config.remotePort, config.localPort, config.sitekey, config.encMethod)
    if (BuildConfig.DEBUG) Log.d(TAG, cmd)
    System.exec(cmd)
  }

  def startDnsDaemon() {
    val cmd = (Path.BASE +
      "ss-tunnel -b 127.0.0.1 -s \"%s\" -p \"%d\" -l \"%d\" -k \"%s\" -m \"%s\" -L 8.8.8.8:53 -u -f " +
      Path.BASE + "ss-local.pid")
      .format(config.proxy, config.remotePort, 8153, config.sitekey, config.encMethod)
    // val cmd: String = Path.BASE + "pdnsd -c " + Path.BASE + "pdnsd.conf"
    // val conf: String = ConfigUtils.PDNSD.format("0.0.0.0")
    // ConfigUtils.printToFile(new File(Path.BASE + "pdnsd.conf"))(p => {
    //   p.println(conf)
    // })
    Utils.runCommand(cmd)
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

    val proxy_address = config.proxy

    val builder = new Builder()
    builder
      .setSession(config.profileName)
      .setMtu(VPN_MTU)
      .addAddress(PRIVATE_VLAN.format("1"), 24)
      .addDnsServer("8.8.8.8")

    if (InetAddressUtils.isIPv6Address(config.proxy)) {
      builder.addRoute("0.0.0.0", 0)
    } else if (config.isGFWList) {
      val gfwList = {
        if (Build.VERSION.SDK_INT == 19) {
          getResources.getStringArray(R.array.simple_list)
        } else {
          getResources.getStringArray(R.array.gfw_list)
        }
      }
      gfwList.foreach(cidr => {
        val net = new SubnetUtils(cidr).getInfo
        if (!net.isInRange(proxy_address)) {
          val addr = cidr.split('/')
          builder.addRoute(addr(0), addr(1).toInt)
        }
      })
    } else {
      for (i <- 1 to 254) {
        if (i != 26 && i != 127) {
          val addr = i.toString + ".0.0.0"
          val cidr = addr + "/8"
          val net = new SubnetUtils(cidr).getInfo

          if (!net.isInRange(proxy_address)) {
            if (!InetAddress.getByName(addr).isSiteLocalAddress) {
              builder.addRoute(addr, 8)
            }
          } else {
            for (j <- 0 to 255) {
              val subAddr = i.toString + "." + j.toString + ".0.0"
              val subCidr = subAddr + "/16"
              val subNet = new SubnetUtils(subCidr).getInfo
              if (!subNet.isInRange(proxy_address)) {
                if (!InetAddress.getByName(subAddr).isSiteLocalAddress) {
                  builder.addRoute(subAddr, 16)
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

    val cmd = (Path.BASE +
      "tun2socks --netif-ipaddr %s "
      + "--dnsgw  %s:8153 "
      + "--netif-netmask 255.255.255.0 "
      + "--socks-server-addr 127.0.0.1:%d "
      + "--tunfd %d "
      + "--tunmtu %d "
      + "--loglevel 3 "
      + "--pid %stun2socks.pid")
      .format(PRIVATE_VLAN.format("2"), PRIVATE_VLAN.format("1"), config.localPort, fd, VPN_MTU,
        Path.BASE)
    if (BuildConfig.DEBUG) Log.d(TAG, cmd)
    System.exec(cmd)
  }

  /** Called when the activity is first created. */
  def handleConnection: Boolean = {
    startVpn()
    startShadowsocksDaemon()
    startDnsDaemon()
    true
  }

  def notifyAlert(title: String, info: String) {
    val openIntent: Intent = new Intent(this, classOf[Shadowsocks])
    openIntent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP)
    val contentIntent: PendingIntent = PendingIntent.getActivity(this, 0, openIntent, 0)
    val builder: NotificationCompat.Builder = new NotificationCompat.Builder(this)
    builder
      .setSmallIcon(R.drawable.ic_stat_shadowsocks)
      .setWhen(0)
      .setTicker(title)
      .setContentTitle(getString(R.string.app_name))
      .setContentText(info)
      .setContentIntent(contentIntent)
      .setAutoCancel(true)
    notificationManager.notify(1, builder.build)
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
    val sb = new StringBuilder

    sb ++= "kill -9 `cat " ++= Path.BASE ++= "ss-local.pid`" ++= "\n"
    sb ++= "killall -9 ss-local" ++= "\n"
    sb ++= "kill -9 `cat " ++= Path.BASE ++= "ss-tunnel.pid`" ++= "\n"
    sb ++= "killall -9 ss-tunnel" ++= "\n"
    sb ++= "kill -9 `cat " ++= Path.BASE ++= "tun2socks.pid`" ++= "\n"
    sb ++= "killall -9 tun2socks" ++= "\n"
    // sb ++= "kill -9 `cat " ++= Path.BASE ++= "pdnsd.pid`" ++= "\n"
    // sb ++= "killall -9 pdnsd" ++= "\n"

    Utils.runCommand(sb.toString())
  }

  override def onStartCommand(intent: Intent, flags: Int, startId: Int): Int = {
    Service.START_STICKY
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

    // start the tracker
    EasyTracker
      .getInstance(this)
      .send(MapBuilder
      .createEvent(TAG, "start", getVersionName, 0L)
      .set(Fields.SESSION_CONTROL, "start")
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
        val container = getApplication.asInstanceOf[ShadowsocksApplication].tagContainer
        try {
          config = ConfigUtils.getPublicConfig(getBaseContext, container, config)
        } catch {
          case ex: Exception =>
            notifyAlert(getString(R.string.forward_fail), getString(R.string.service_failed))
            stopRunner()
            changeState(State.STOPPED)
            return
        }
      }

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
        notifyAlert(getString(R.string.forward_fail), getString(R.string.service_failed))
        changeState(State.STOPPED)
        stopRunner()
      }
    }
  }

  override def stopRunner() {

    // channge the state
    changeState(State.STOPPED)

    // stop the tracker
    EasyTracker
      .getInstance(this)
      .send(MapBuilder
      .createEvent(TAG, "stop", getVersionName, 0L)
      .set(Fields.SESSION_CONTROL, "stop")
      .build())

    // clean up the context
    if (receiver != null) {
      unregisterReceiver(receiver)
      receiver = null
    }

    // reset VPN
    killProcesses()

    // close connections
    if (conn != null) {
      conn.close()
      conn = null
    }

    // reset notifications
    notificationManager.cancel(1)

    // stop the service if no callback registered
    if (callbackCount == 0) {
      stopSelf()
    }
  }

  override def stopBackgroundService() {
    stopRunner()
    stopSelf()
  }

  override def getTag = TAG
  override def getServiceMode = Mode.VPN
  override def getContext = getBaseContext
}
