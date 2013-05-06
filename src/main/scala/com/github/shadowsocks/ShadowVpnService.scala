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

import android.app.Notification
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import android.os._
import android.preference.PreferenceManager
import android.support.v4.app.NotificationCompat
import android.util.Log
import com.google.analytics.tracking.android.EasyTracker
import java.io._
import java.lang.ref.WeakReference
import java.net.Inet6Address
import java.net.InetAddress
import java.net.UnknownHostException
import android.net.VpnService

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
        var resolved: Boolean = false
        if (appHost != null) {
          var addr: InetAddress = null
          val isIPv6Support: Boolean = Utils.isIPv6Support

          try {
            val addrs = InetAddress.getAllByName(appHost)
            for (a <- addrs) {
              if (isIPv6Support && addr == null && a.isInstanceOf[Inet6Address]) {
                addr = a
              }
            }
            if (addr == null) addr = addrs(0)
          } catch {
            case ignored: UnknownHostException => {
              addr = null
            }
          }
          if (addr != null) {
            appHost = addr.getHostAddress
            resolved = true
          }
        }
        Log.d(TAG, "IPTABLES: " + Utils.getIptables)
        hasRedirectSupport = Utils.getHasRedirectSupport
        if (resolved && handleConnection) {
          notifyAlert(getString(R.string.forward_success), getString(R.string.service_running))
          handler.sendEmptyMessageDelayed(MSG_CONNECT_SUCCESS, 500)
        }
        else {
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

    val prefix = appHost.substring(0, appHost.indexOf('.')).toInt
    val builder = new Builder()
    builder
      .setSession(getString(R.string.app_name))
      .setMtu(VPN_MTU)
      .addAddress("172.16.0.1", 24)
      .addDnsServer("8.8.8.8")

    for (i <- 1 to 254) {
      if (i != prefix && i != 127
        && i != 172 && i != 192 && i != 10) builder.addRoute(i + ".0.0.0", 8)
    }

    conn = builder.establish()
    if (conn == null) {
      stopSelf()
      return
    }

    val fd = conn.getFd

    val cmd = (BASE + "tun2socks --netif-ipaddr 172.16.0.2  --udpgw-remote-server-addr 158.255.208.201:7300 " +
      "--netif-netmask 255.255.255.0 --socks-server-addr 127.0.0.1:%d --tunfd %d --tunmtu %d --pid " + BASE + "tun2socks.pid")
      .format(localPort, fd, VPN_MTU)

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
    val closeIntent: Intent = new Intent(Utils.CLOSE_ACTION)
    val actionIntent: PendingIntent = PendingIntent.getBroadcast(this, 0, closeIntent, 0)
    val builder: NotificationCompat.Builder = new NotificationCompat.Builder(this)
    builder.setSmallIcon(R.drawable.ic_stat_shadowsocks).setWhen(0).setTicker(title).setContentTitle(getString(R.string.app_name)).setContentText(info).setContentIntent(contentIntent).addAction(android.R.drawable.ic_menu_close_clear_cancel, getString(R.string.stop), actionIntent)
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
  }

  /** Called when the activity is closed. */
  override def onDestroy() {
    EasyTracker.getTracker.sendEvent("service", "stop", getVersionName, 0L)
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
    super.onDestroy()
    markServiceStopped()
  }

  def onDisconnect() {
    Utils.runRootCommand(Utils.getIptables + " -t nat -F OUTPUT")
    val sb = new StringBuilder
    sb.append("kill -9 `cat /data/data/com.github.shadowsocks/redsocks.pid`").append("\n")
    if (!waitForProcess("shadowsocks")) {
      sb.append("kill -9 `cat /data/data/com.github.shadowsocks/shadowsocks.pid`").append("\n")
    }
    if (!waitForProcess("tun2socks")) {
      sb.append("kill -9 `cat /data/data/com.github.shadowsocks/tun2socks.pid`").append("\n")
    }
    Utils.runRootCommand(sb.toString())
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
  var appHost: String = null
  var remotePort: Int = 0
  var localPort: Int = 0
  var sitekey: String = null
  var settings: SharedPreferences = null
  var hasRedirectSupport: Boolean = true
  var isGlobalProxy: Boolean = false
  var isGFWList: Boolean = false
  var isBypassApps: Boolean = false
  var isDNSProxy: Boolean = false
  var isHTTPProxy: Boolean = false
  var encMethod: String = null
  var apps: Array[ProxiedApp] = null
}