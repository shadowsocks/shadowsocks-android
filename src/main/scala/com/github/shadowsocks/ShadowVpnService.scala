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

import android.app._
import android.content._
import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import android.os._
import android.support.v4.app.NotificationCompat
import android.util.Log
import com.google.analytics.tracking.android.EasyTracker
import java.io._
import android.net.VpnService
import org.apache.http.conn.util.InetAddressUtils
import android.os.Message
import scala.Some
import scala.concurrent.ops._

object ShadowVpnService {
  def isServiceStarted(context: Context): Boolean = {
    Utils.isServiceStarted("com.github.shadowsocks.ShadowVpnService", context)
  }
}

class ShadowVpnService extends VpnService {

  val TAG = "ShadowVpnService"
  val BASE = "/data/data/com.github.shadowsocks/"
  val SHADOWSOCKS_CONF = "{\"server\": [%s], \"server_port\": %d, \"local_port\": %d, \"password\": %s, \"timeout\": %d}"
  val MSG_CONNECT_FINISH = 1
  val MSG_CONNECT_SUCCESS = 2
  val MSG_CONNECT_FAIL = 3
  val MSG_STOP_SELF = 5
  val MSG_VPN_ERROR = 6

  val VPN_MTU = 1500

  val PRIVATE_VLAN_10 = "10.254.254.%s"
  val PRIVATE_VLAN_172 = "172.30.254.%s"

  var conn: ParcelFileDescriptor = null
  var notificationManager: NotificationManager = null
  var receiver: BroadcastReceiver = null
  var apps: Array[ProxiedApp] = null
  var config: Config = null

  private var state = State.INIT
  private var message: String = null

  def changeState(s: Int) {
    changeState(s, null)
  }

  def changeState(s: Int, m: String) {
    if (state != s) {
      state = s
      if (m != null) message = m
      val intent = new Intent(Action.UPDATE_STATE)
      intent.putExtra(Extra.STATE, state)
      intent.putExtra(Extra.MESSAGE, message)
      sendBroadcast(intent)
    }
  }

  val handler: Handler = new Handler {
    override def handleMessage(msg: Message) {
      msg.what match {
        case MSG_CONNECT_SUCCESS =>
          changeState(State.CONNECTED)
        case MSG_CONNECT_FAIL =>
          changeState(State.STOPPED)
        case MSG_VPN_ERROR =>
          if (msg.obj != null) changeState(State.STOPPED, msg.obj.asInstanceOf[String])
        case MSG_STOP_SELF =>
          destroy()
          stopSelf()
        case _ =>
      }
      super.handleMessage(msg)
    }
  }

  def getPid(name: String): Int = {
    try {
      val reader: BufferedReader = new BufferedReader(new FileReader(BASE + name + ".pid"))
      val line = reader.readLine
      return Integer.valueOf(line)
    } catch {
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
    spawn {
      val cmd: String = (BASE +
        "shadowsocks -s \"%s\" -p \"%d\" -l \"%d\" -k \"%s\" -m \"%s\" -f " +
        BASE +
        "shadowsocks.pid")
        .format(config.proxy, config.remotePort, config.localPort, config.sitekey, config.encMethod)
      if (BuildConfig.DEBUG) Log.d(TAG, cmd)
      System.exec(cmd)
    }
  }

  def startDnsDaemon() {
    val cmd: String = BASE + "pdnsd -c " + BASE + "pdnsd.conf"
    Utils.runCommand(cmd)
  }

  def getVersionName: String = {
    var version: String = null
    try {
      val pi: PackageInfo = getPackageManager.getPackageInfo(getPackageName, 0)
      version = pi.versionName
    } catch {
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

    if (VpnService.prepare(this) != null) {
      val i = new Intent(this, classOf[ShadowVpnActivity])
      i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
      startActivity(i)
      stopSelf()
      return
    }

    changeState(State.CONNECTING)

    config = Extra.get(intent)


    spawn {
      killProcesses()

      // Resolve server address
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
        handler.sendEmptyMessageDelayed(MSG_CONNECT_SUCCESS, 300)
      } else {
        notifyAlert(getString(R.string.forward_fail), getString(R.string.service_failed))
        handler.sendEmptyMessageDelayed(MSG_CONNECT_FAIL, 300)
        handler.sendEmptyMessageDelayed(MSG_STOP_SELF, 500)
      }
      handler.sendEmptyMessageDelayed(MSG_CONNECT_FINISH, 300)
    }
  }

  def waitForProcess(name: String): Boolean = {
    val pid: Int = getPid(name)
    if (pid == -1) return false
    Exec.hangupProcessGroup(-pid)
    val t: Thread = new Thread {
      override def run() {
        Exec.waitFor(-pid)
      }
    }
    t.start()
    try {
      t.join(300)
    } catch {
      case ignored: InterruptedException => {
      }
    }
    !t.isAlive
  }

  def startVpn() {

    val address = config.proxy.split('.')
    val prefix1 = address(0)
    val prefix2 = address.slice(0, 2).mkString(".")
    val prefix3 = address.slice(0, 3).mkString(".")

    val localAddress = Utils.getIPv4Address match {
      case Some(ip) if ip.split('.')(0) == "172" => PRIVATE_VLAN_10
      case _ => PRIVATE_VLAN_172
    }

    val builder = new Builder()
    builder
      .setSession(getString(R.string.app_name))
      .setMtu(VPN_MTU)
      .addAddress(localAddress.format("1"), 24)
      .addDnsServer("8.8.8.8")

    if (InetAddressUtils.isIPv6Address(config.proxy)) {
      builder.addRoute("0.0.0.0", 0)
    } else if (config.isGFWList) {
      val gfwList = getResources.getStringArray(R.array.gfw_list)
      gfwList.foreach(addr =>
        if (addr != prefix2) {
          builder.addRoute(addr + ".0.0", 16)
        } else {
          for (i <- 0 to 255) {
            val prefix = Array(addr, i.toString).mkString(".")
            if (prefix != prefix3) builder.addRoute(prefix + ".0", 24)
          }
        })
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

    try {
      conn = builder.establish()
    } catch {
      case ex: IllegalStateException => {
        val msg = new Message()
        msg.what = MSG_VPN_ERROR
        msg.obj = ex.getMessage
        handler.sendMessage(msg)
        conn = null
      }
      case ex: Exception => conn = null
    }

    if (conn == null) {
      stopSelf()
      return
    }

    val fd = conn.getFd

    val cmd = (BASE +
      "tun2socks --netif-ipaddr %s "
      // + "--udpgw-remote-server-addr %s:7300 "
      + "--dnsgw  %s:8153 "
      + "--netif-netmask 255.255.255.0 "
      + "--socks-server-addr 127.0.0.1:%d "
      + "--tunfd %d "
      + "--tunmtu %d "
      + "--loglevel 3 "
      + "--pid %stun2socks.pid")
      .format(localAddress.format("2"), localAddress.format("1"), config.localPort, fd, VPN_MTU, BASE)
    if (BuildConfig.DEBUG) Log.d(TAG, cmd)
    System.exec(cmd)
  }

  /** Called when the activity is first created. */
  def handleConnection: Boolean = {
    startDnsDaemon()
    startShadowsocksDaemon()
    startVpn()
    true
  }

  def initSoundVibrateLights(notification: Notification) {
    notification.sound = null
    notification.defaults |= Notification.DEFAULT_LIGHTS
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
    }
    null
  }

  override def onCreate() {
    super.onCreate()
    EasyTracker.getTracker.setStartSession(true)
    EasyTracker.getTracker.sendEvent(TAG, "start", getVersionName, 0L)
    notificationManager = getSystemService(Context.NOTIFICATION_SERVICE)
      .asInstanceOf[NotificationManager]

    // register close receiver
    val filter = new IntentFilter()
    filter.addAction(Intent.ACTION_SHUTDOWN)
    filter.addAction(Action.CLOSE)
    receiver = new BroadcastReceiver {
      def onReceive(p1: Context, p2: Intent) {
        destroy()
        stopSelf()
      }
    }
    registerReceiver(receiver, filter)
  }

  def destroy() {
    killProcesses()

    changeState(State.STOPPED)
    EasyTracker.getTracker.sendEvent(TAG, "stop", getVersionName, 0L)
    if (receiver != null) {
      unregisterReceiver(receiver)
      receiver = null
    }
    if (conn != null) {
      conn.close()
      conn = null
    }
    notificationManager.cancel(1)
  }

  /** Called when the activity is closed. */
  override def onDestroy() {
    EasyTracker.getTracker.setStartSession(false)
    EasyTracker.getTracker.sendEvent(TAG, "stop", getVersionName, 0L)
    destroy()
    super.onDestroy()
  }

  def killProcesses() {
    val sb = new StringBuilder
    if (!waitForProcess("shadowsocks")) {
      sb ++= "kill -9 `cat /data/data/com.github.shadowsocks/shadowsocks.pid`" ++= "\n"
      sb ++= "killall -9 shadowsocks" ++= "\n"
    }
    if (!waitForProcess("tun2socks")) {
      sb ++= "kill -9 `cat /data/data/com.github.shadowsocks/tun2socks.pid`" ++= "\n"
      sb ++= "killall -9 tun2socks" ++= "\n"
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
}
