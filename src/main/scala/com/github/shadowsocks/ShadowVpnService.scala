/* Shadowsocks - A shadowsocks client for Android
 * Copyright (C) 2012 <max.c.lv@gmail.com>
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

import android.app.{NotificationManager, Notification, PendingIntent, Service}
import android.content._
import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import android.os._
import android.preference.PreferenceManager
import android.support.v4.app.NotificationCompat
import android.util.Log
import com.google.analytics.tracking.android.EasyTracker
import java.io._
import java.lang.ref.WeakReference
import android.net.VpnService
import org.apache.http.conn.util.InetAddressUtils
import org.xbill.DNS._
import android.os.Message
import scala.Some

object ShadowVpnService {
  def isServiceStarted: Boolean = {
    if (sRunningInstance == null) {
      false
    } else if (sRunningInstance.get == null) {
      sRunningInstance = null
      false
    } else {
      true
    }
  }

  var sRunningInstance: WeakReference[ShadowVpnService] = null

}

class ShadowVpnService extends VpnService {

  val TAG = "ShadowVpnService"
  val BASE = "/data/data/com.github.shadowsocks/"
  val SHADOWSOCKS_CONF = "{\"server\": [%s], \"server_port\": %d, \"local_port\": %d, \"password\": %s, \"timeout\": %d}"
  val MSG_CONNECT_START: Int = 0
  val MSG_CONNECT_FINISH: Int = 1
  val MSG_CONNECT_SUCCESS: Int = 2
  val MSG_CONNECT_FAIL: Int = 3
  val MSG_HOST_CHANGE: Int = 4
  val MSG_STOP_SELF: Int = 5
  val VPN_MTU = 1500

  var conn: ParcelFileDescriptor = null
  var udpgw: String = null

  def getPid(name: String): Int = {
    try {
      val reader: BufferedReader = new BufferedReader(new FileReader(BASE + name + ".pid"))
      val line = reader.readLine
      return Integer.valueOf(line)
    }
    catch {
      case e: FileNotFoundException => {
        Log.e(TAG, "Cannot open pid file: " + name)
      }
      case e: IOException => {
        Log.e(TAG, "Cannot read pid file: " + name)
      }
      case e: NumberFormatException => {
        Log.e(TAG, "Invalid pid", e)
      }
    }
    -1
  }

  def startShadowsocksDaemon() {
    new Thread {
      override def run() {
        val cmd: String = (BASE + "shadowsocks -s \"%s\" -p \"%d\" -l \"%d\" -k \"%s\" -m \"%s\" -f " + BASE + "shadowsocks.pid")
          .format(appHost, remotePort, localPort, sitekey, encMethod)
        System.exec(cmd)
      }
    }.start()
  }

  def resolve(host: String, addrType: Int): Option[String] = {
    val lookup = new Lookup(host, addrType)
    val resolver = new SimpleResolver("8.8.8.8")
    resolver.setTimeout(5)
    lookup.setResolver(resolver)
    val records = lookup.run()
    if (records == null) return None
    for (r <- records) {
      addrType match {
        case Type.A =>
          return Some(r.asInstanceOf[ARecord].getAddress.getHostAddress)
        case Type.AAAA =>
          return Some(r.asInstanceOf[AAAARecord].getAddress.getHostAddress)
      }
    }
    None
  }

  def getVersionName: String = {
    var version: String = null
    try {
      val pi: PackageInfo = getPackageManager.getPackageInfo(getPackageName, 0)
      version = pi.versionName
    }
    catch {
      case e: PackageManager.NameNotFoundException => {
        version = "Package name not found"
      }
    }
    version
  }

  def handleCommand(intent: Intent) {
    if (intent == null) {
      stopSelf()
      return
    }
    appHost = settings.getString("proxy", "127.0.0.1")
    sitekey = settings.getString("sitekey", "default")
    encMethod = settings.getString("encMethod", "table")
    try {
      remotePort = Integer.valueOf(settings.getString("remotePort", "1984"))
    }
    catch {
      case ex: NumberFormatException => {
        remotePort = 1984
      }
    }
    try {
      localPort = Integer.valueOf(settings.getString("port", "1984"))
    }
    catch {
      case ex: NumberFormatException => {
        localPort = 1984
      }
    }
    isGlobalProxy = settings.getBoolean("isGlobalProxy", false)
    isGFWList = settings.getBoolean("isGFWList", false)
    isBypassApps = settings.getBoolean("isBypassApps", false)
    isDNSProxy = settings.getBoolean("isDNSProxy", false)
    isHTTPProxy = settings.getBoolean("isHTTPProxy", false)
    if (isHTTPProxy) {
      localPort -= 1
    }
    new Thread(new Runnable {
      def run() {
        handler.sendEmptyMessage(MSG_CONNECT_START)

        // Resolve server address
        var resolved: Boolean = false
        if (!InetAddressUtils.isIPv4Address(appHost) && !InetAddressUtils.isIPv6Address(appHost)) {
          if (Utils.isIPv6Support) {
            resolve(appHost, Type.AAAA) match {
              case Some(host) => {
                appHost = host
                resolved = true
              }
              case None =>
            }
          }
          if (!resolved) {
            resolve(appHost, Type.A) match {
              case Some(host) => {
                appHost = host
                resolved = true
              }
              case None =>
            }
          }
        } else {
          resolved = true
        }

        // Resolve UDP gateway
        if (resolved) {
          resolve("u.maxcdn.info", Type.A) match {
            case Some(host) => udpgw = host
            case None => resolved = false
          }
        }

        if (resolved && handleConnection) {
          handler.sendEmptyMessageDelayed(MSG_CONNECT_SUCCESS, 500)
        } else {
          notifyAlert(getString(R.string.forward_fail), getString(R.string.service_failed))
          stopSelf()
          handler.sendEmptyMessageDelayed(MSG_CONNECT_FAIL, 500)
        }
        handler.sendEmptyMessageDelayed(MSG_CONNECT_FINISH, 500)
      }
    }).start()
    markServiceStarted()
  }

  def waitForProcess(name: String): Boolean = {
    val pid: Int = getPid(name)
    if (pid == -1) return false
    Exec.hangupProcessGroup(-pid)
    val t: Thread = new Thread {
      override def run() {
        Exec.waitFor(-pid)
        Log.d(TAG, "Successfully exit pid: " + pid)
      }
    }
    t.start()
    try {
      t.join(300)
    }
    catch {
      case ignored: InterruptedException => {
      }
    }
    !t.isAlive
  }

  def startVpn() {

    val address = appHost.split('.')
    val prefix1 = address(0)
    val prefix2 = address.slice(0, 2).mkString(".")
    val prefix3 = address.slice(0, 3).mkString(".")

    val builder = new Builder()
    builder
      .setSession(getString(R.string.app_name))
      .setMtu(VPN_MTU)
      .addAddress("172.16.0.1", 24)
      .addDnsServer("8.8.8.8")
      .addDnsServer("8.8.4.4")

    if (InetAddressUtils.isIPv6Address(appHost)) {
      builder.addRoute("0.0.0.0", 0)
    } else if (isGFWList) {
      val gfwList = getResources.getStringArray(R.array.gfw_list)
      gfwList.foreach(addr =>
        if (addr != prefix2) {
          builder.addRoute(addr + ".0.0", 16)
        } else {
          for (i <- 0 to 255) {
            val prefix = Array(addr, i.toString).mkString(".")
            if (prefix != prefix3) builder.addRoute(prefix + ".0", 24)
          }
        }
      )
      builder.addRoute("8.8.0.0", 16)
    } else {
      for (i <- 1 to 254) {
        if (i != 127 && i != 172 && i != 192 && i != 10 && i.toString != prefix1) {
          builder.addRoute(i + ".0.0.0", 8)
        } else if (i.toString == prefix1) {
          for (j <- 0 to 255) {
            val prefix = Array(i.toString, j.toString).mkString(".")
            if (prefix != prefix2) builder.addRoute(prefix + ".0.0", 16)
          }
        }
        builder.addRoute("8.8.0.0", 16)
      }
    }

    conn = builder.establish()
    if (conn == null) {
      stopSelf()
      return
    }

    val fd = conn.getFd

    val cmd = (BASE + "tun2socks --netif-ipaddr 172.16.0.2  --udpgw-remote-server-addr %s:7300 " +
      "--netif-netmask 255.255.255.0 --socks-server-addr 127.0.0.1:%d --tunfd %d --tunmtu %d --pid " + BASE + "tun2socks.pid")
      .format(udpgw, localPort, fd, VPN_MTU)

    Log.d(TAG, cmd)
    System.exec(cmd)
  }

  /** Called when the activity is first created. */
  def handleConnection: Boolean = {
    startShadowsocksDaemon()
    startVpn()
    true
  }

  def initSoundVibrateLights(notification: Notification) {
    notification.sound = null
    notification.defaults |= Notification.DEFAULT_LIGHTS
  }

  def markServiceStarted() {
    ShadowVpnService.sRunningInstance = new WeakReference[ShadowVpnService](this)
  }

  def markServiceStopped() {
    ShadowVpnService.sRunningInstance = null
  }

  def notifyAlert(title: String, info: String) {
    val openIntent: Intent = new Intent(this, classOf[Shadowsocks])
    openIntent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP)
    val contentIntent: PendingIntent = PendingIntent.getActivity(this, 0, openIntent, 0)
    val builder: NotificationCompat.Builder = new NotificationCompat.Builder(this)
    builder
      .setSmallIcon(R.drawable.ic_stat_shadowsocks).setWhen(0)
      .setTicker(title).setContentTitle(getString(R.string.app_name))
      .setContentText(info).setContentIntent(contentIntent)
      .setAutoCancel(true)
    notificationManager.notify(1, builder.build)
  }

  override def onBind(intent: Intent): IBinder = {
    val action = intent.getAction
    if (VpnService.SERVICE_INTERFACE == action) {
      return super.onBind(intent)
    }
    null
  }

  override def onCreate() {
    super.onCreate()
    EasyTracker.getTracker.sendEvent("service", "start", getVersionName, 0L)
    settings = PreferenceManager.getDefaultSharedPreferences(this)

    notificationManager = getSystemService(Context.NOTIFICATION_SERVICE).asInstanceOf[NotificationManager]

    // register close receiver
    val filter = new IntentFilter()
    filter.addAction(Intent.ACTION_SHUTDOWN)
    filter.addAction(Utils.CLOSE_ACTION)
    receiver = new CloseReceiver
    registerReceiver(receiver, filter)
  }

  def destroy() {
    EasyTracker.getTracker.sendEvent("service", "stop", getVersionName, 0L)
    if (receiver != null) unregisterReceiver(receiver)
    new Thread {
      override def run() {
        onDisconnect()
      }
    }.start()
    val ed: SharedPreferences.Editor = settings.edit
    ed.putBoolean("isRunning", false)
    ed.putBoolean("isConnecting", false)
    ed.commit
    if (conn != null) {
      conn.close()
      conn = null
    }
    markServiceStopped()
  }

  /** Called when the activity is closed. */
  override def onDestroy() {
    destroy()
    super.onDestroy()
  }

  def onDisconnect() {
    val sb = new StringBuilder
    if (!waitForProcess("redsocks")) {
      sb.append("kill -9 `cat /data/data/com.github.shadowsocks/redsocks.pid`").append("\n")
    }
    if (!waitForProcess("shadowsocks")) {
      sb.append("kill -9 `cat /data/data/com.github.shadowsocks/shadowsocks.pid`").append("\n")
    }
    if (!waitForProcess("tun2socks")) {
      sb.append("kill -9 `cat /data/data/com.github.shadowsocks/tun2socks.pid`").append("\n")
    }
    Utils.runCommand(sb.toString())
  }

  override def onStart(intent: Intent, startId: Int) {
    handleCommand(intent)
  }

  override def onStartCommand(intent: Intent, flags: Int, startId: Int): Int = {
    handleCommand(intent)
    Service.START_STICKY
  }

  override def onRevoke() {
    stopSelf()
  }

  class CloseReceiver extends BroadcastReceiver {
    def onReceive(p1: Context, p2: Intent) {
      destroy()
      stopSelf()
    }
  }

  val handler: Handler = new Handler {
    override def handleMessage(msg: Message) {
      val ed: SharedPreferences.Editor = settings.edit
      msg.what match {
        case MSG_CONNECT_START =>
          ed.putBoolean("isConnecting", true)
          val pm: PowerManager = getSystemService(Context.POWER_SERVICE).asInstanceOf[PowerManager]
        case MSG_CONNECT_FINISH =>
          ed.putBoolean("isConnecting", false)
        case MSG_CONNECT_SUCCESS =>
          ed.putBoolean("isRunning", true)
        case MSG_CONNECT_FAIL =>
          ed.putBoolean("isRunning", false)
        case MSG_HOST_CHANGE =>
          ed.putString("appHost", appHost)
        case MSG_STOP_SELF =>
          stopSelf()
      }
      ed.commit
      super.handleMessage(msg)
    }
  }

  var notificationManager: NotificationManager = null
  var receiver: CloseReceiver = null
  var appHost: String = null
  var remotePort: Int = 0
  var localPort: Int = 0
  var sitekey: String = null
  var settings: SharedPreferences = null
  var isGlobalProxy: Boolean = false
  var isGFWList: Boolean = false
  var isBypassApps: Boolean = false
  var isDNSProxy: Boolean = false
  var isHTTPProxy: Boolean = false
  var encMethod: String = null
  var apps: Array[ProxiedApp] = null
}