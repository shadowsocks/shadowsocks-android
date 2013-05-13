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
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content._
import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import android.os.Handler
import android.os.IBinder
import android.os.Message
import android.os.PowerManager
import android.preference.PreferenceManager
import android.support.v4.app.NotificationCompat
import android.util.Log
import com.google.analytics.tracking.android.EasyTracker
import java.io.BufferedReader
import java.io.FileNotFoundException
import java.io.FileReader
import java.io.IOException
import java.lang.ref.WeakReference
import java.lang.reflect.InvocationTargetException
import java.lang.reflect.Method
import org.apache.http.conn.util.InetAddressUtils
import scala.collection._
import org.xbill.DNS._
import scala.Some
import scala.Some
import java.net.{UnknownHostException, InetAddress}

object ShadowsocksService {
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

  var sRunningInstance: WeakReference[ShadowsocksService] = null
}

class ShadowsocksService extends Service {

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

  def startPolipoDaemon() {
    new Thread {
      override def run() {
        val cmd: String = (BASE + "polipo proxyPort=%d socksParentProxy=127.0.0.1:%d daemonise=true pidFile=\"%s\" logLevel=1 logFile=\"%s\"")
          .format(localPort + 1, localPort, BASE + "polipo.pid", BASE + "polipo.log")
        System.exec(cmd)
      }
    }.start()
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
        if (!InetAddressUtils.isIPv4Address(appHost) && !InetAddressUtils.isIPv6Address(appHost)) {
          Utils.resolve(appHost, enableIPv6 = true) match {
            case Some(addr) =>
              appHost = addr
              resolved = true
            case None => resolved = false
          }
        } else {
          resolved = true
        }

        Log.d(TAG, "IPTABLES: " + Utils.getIptables)
        hasRedirectSupport = Utils.getHasRedirectSupport
        if (resolved && handleConnection) {
          notifyForegroundAlert(getString(R.string.forward_success), getString(R.string.service_running))
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

  def startRedsocksDaemon() {
    val conf = REDSOCKS_CONF.format(localPort)
    val cmd = "%sredsocks -p %sredsocks.pid -c %sredsocks.conf".format(BASE, BASE, BASE)
    Utils.runRootCommand("echo \"" + conf + "\" > " + BASE + "redsocks.conf\n" + cmd)
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

  /** Called when the activity is first created. */
  def handleConnection: Boolean = {
    if (isHTTPProxy) {
      startPolipoDaemon()
    }
    startShadowsocksDaemon()
    startDnsDaemon()
    startRedsocksDaemon()
    setupIptables
    true
  }

  def initSoundVibrateLights(notification: Notification) {
    notification.sound = null
    notification.defaults |= Notification.DEFAULT_LIGHTS
  }

  def invokeMethod(method: Method, args: Array[AnyRef]) {
    try {
      method.invoke(this, mStartForegroundArgs: _*)
    }
    catch {
      case e: InvocationTargetException => {
        Log.w(TAG, "Unable to invoke method", e)
      }
      case e: IllegalAccessException => {
        Log.w(TAG, "Unable to invoke method", e)
      }
    }
  }

  def markServiceStarted() {
    ShadowsocksService.sRunningInstance = new WeakReference[ShadowsocksService](this)
  }

  def markServiceStopped() {
    ShadowsocksService.sRunningInstance = null
  }

  def notifyForegroundAlert(title: String, info: String) {
    val openIntent: Intent = new Intent(this, classOf[Shadowsocks])
    openIntent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP)
    val contentIntent: PendingIntent = PendingIntent.getActivity(this, 0, openIntent, 0)
    val closeIntent: Intent = new Intent(Utils.CLOSE_ACTION)
    val actionIntent: PendingIntent = PendingIntent.getBroadcast(this, 0, closeIntent, 0)
    val builder: NotificationCompat.Builder = new NotificationCompat.Builder(this)
    builder
      .setSmallIcon(R.drawable.ic_stat_shadowsocks).setWhen(0)
      .setTicker(title).setContentTitle(getString(R.string.app_name))
      .setContentText(info).setContentIntent(contentIntent)
      .addAction(android.R.drawable.ic_menu_close_clear_cancel, getString(R.string.stop), actionIntent)
    startForegroundCompat(1, builder.build)
  }

  def notifyAlert(title: String, info: String) {
    val openIntent: Intent = new Intent(this, classOf[Shadowsocks])
    openIntent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP)
    val contentIntent: PendingIntent = PendingIntent.getActivity(this, 0, openIntent, 0)
    val builder: NotificationCompat.Builder = new NotificationCompat.Builder(this)
    builder
      .setSmallIcon(R.drawable.ic_stat_shadowsocks).setWhen(0)
      .setTicker(title)
      .setContentTitle(getString(R.string.app_name))
      .setContentText(info).setContentIntent(contentIntent)
      .setAutoCancel(true)
    notificationManager.notify(1, builder.build)
  }

  def onBind(intent: Intent): IBinder = {
    null
  }

  override def onCreate() {
    super.onCreate()
    EasyTracker.getTracker.sendEvent("service", "start", getVersionName, 0L)
    settings = PreferenceManager.getDefaultSharedPreferences(this)
    notificationManager = this.getSystemService(Context.NOTIFICATION_SERVICE).asInstanceOf[NotificationManager]
    try {
      mStartForeground = getClass.getMethod("startForeground", mStartForegroundSignature: _*)
      mStopForeground = getClass.getMethod("stopForeground", mStopForegroundSignature: _*)
    }
    catch {
      case e: NoSuchMethodException => {
        mStartForeground = ({
          mStopForeground = null
          mStopForeground
        })
      }
    }
    try {
      mSetForeground = getClass.getMethod("setForeground", mSetForegroundSignature: _*)
    }
    catch {
      case e: NoSuchMethodException => {
        throw new IllegalStateException("OS doesn't have Service.startForeground OR Service.setForeground!")
      }
    }

    // register close receiver
    val filter = new IntentFilter()
    filter.addAction(Intent.ACTION_SHUTDOWN)
    filter.addAction(Utils.CLOSE_ACTION)
    receiver = new BroadcastReceiver() {
      def onReceive(p1: Context, p2: Intent) { stopSelf() }
    }
    registerReceiver(receiver, filter)

  }

  /** Called when the activity is closed. */
  override def onDestroy() {
    EasyTracker.getTracker.sendEvent("service", "stop", getVersionName, 0L)
    stopForegroundCompat(1)
    new Thread {
      override def run() {
        onDisconnect()
      }
    }.start()

    val ed: SharedPreferences.Editor = settings.edit
    ed.putBoolean("isRunning", false)
    ed.putBoolean("isConnecting", false)
    ed.commit
    markServiceStopped()

    if (receiver != null) {
      unregisterReceiver(receiver)
      receiver = null
    }

    super.onDestroy()
  }

  def onDisconnect() {
    Utils.runRootCommand(Utils.getIptables + " -t nat -F OUTPUT")

    val sb = new StringBuilder
    sb ++= "kill -9 `cat /data/data/com.github.shadowsocks/redsocks.pid`" ++= "\n"
    sb ++= "killall -9 redsocks" ++= "\n"
    Utils.runRootCommand(sb.toString())

    sb.clear()
    if (!waitForProcess("pdnsd")) {
      sb ++= "kill -9 `cat /data/data/com.github.shadowsocks/pdnsd.pid`" ++= "\n"
      sb ++= "killall -9 pdnsd" ++= "\n"
    }
    if (!waitForProcess("shadowsocks")) {
      sb ++= "kill -9 `cat /data/data/com.github.shadowsocks/shadowsocks.pid`" ++= "\n"
      sb ++= "killall -9 shadowsocks" ++= "\n"
    }
    if (isHTTPProxy) {
      if (!waitForProcess("polipo")) {
        sb ++= "kill -9 `cat /data/data/com.github.shadowsocks/polipo.pid`" ++= "\n"
        sb ++= "killall -9 polipo" ++= "\n"
      }
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

  def setupIptables: Boolean = {
    val init_sb = new StringBuilder
    val http_sb = new StringBuilder
    init_sb.append(Utils.getIptables).append(" -t nat -F OUTPUT\n")
    val cmd_bypass = Utils.getIptables + CMD_IPTABLES_RETURN
    if (!InetAddressUtils.isIPv6Address(appHost.toUpperCase)) {
      init_sb.append(cmd_bypass.replace("-d 0.0.0.0", "-d " + appHost))
    }
    init_sb.append(cmd_bypass.replace("0.0.0.0", "127.0.0.1"))
    if (!isDNSProxy) {
      init_sb.append(cmd_bypass.replace("-d 0.0.0.0", "--dport " + 53))
      init_sb.append(cmd_bypass.replace("-d 0.0.0.0", "-m owner --uid-owner " + getApplicationInfo.uid))
    }
    if (hasRedirectSupport) {
      init_sb.append(Utils.getIptables).append(" -t nat -A OUTPUT -p udp --dport 53 -j REDIRECT --to ").append(DNS_PORT).append("\n")
    }
    else {
      init_sb.append(Utils.getIptables).append(" -t nat -A OUTPUT -p udp --dport 53 -j DNAT --to-destination 127.0.0.1:").append(DNS_PORT).append("\n")
    }
    if (isGFWList) {
      val chn_list: Array[String] = getResources.getStringArray(R.array.chn_list)
      for (item <- chn_list) {
        init_sb.append(cmd_bypass.replace("0.0.0.0", item))
      }
    }
    if (isGlobalProxy || isBypassApps) {
      http_sb.append(if (hasRedirectSupport) Utils.getIptables + CMD_IPTABLES_REDIRECT_ADD_SOCKS else Utils.getIptables + CMD_IPTABLES_DNAT_ADD_SOCKS)
    }
    if (!isGlobalProxy) {
      if (apps == null || apps.length <= 0) apps = AppManager.getProxiedApps(this)
      val uidSet: mutable.HashSet[Int] = new mutable.HashSet[Int]
      for (app <- apps) {
        if (app.proxied) {
          uidSet.add(app.uid)
        }
      }
      for (uid <- uidSet) {
        if (!isBypassApps) {
          http_sb.append((if (hasRedirectSupport) Utils.getIptables + CMD_IPTABLES_REDIRECT_ADD_SOCKS else Utils.getIptables + CMD_IPTABLES_DNAT_ADD_SOCKS).replace("-t nat", "-t nat -m owner --uid-owner " + uid))
        }
        else {
          init_sb.append(cmd_bypass.replace("-d 0.0.0.0", "-m owner --uid-owner " + uid))
        }
      }
    }
    val init_rules: String = init_sb.toString()
    Utils.runRootCommand(init_rules, 30 * 1000)
    val redt_rules: String = http_sb.toString()
    Utils.runRootCommand(redt_rules)
    true
  }

  /**
   * This is a wrapper around the new startForeground method, using the older
   * APIs if it is not available.
   */
  def startForegroundCompat(id: Int, notification: Notification) {
    if (mStartForeground != null) {
      mStartForegroundArgs(0) = int2Integer(id)
      mStartForegroundArgs(1) = notification
      invokeMethod(mStartForeground, mStartForegroundArgs)
      return
    }
    mSetForegroundArgs(0) = boolean2Boolean(x = true)
    invokeMethod(mSetForeground, mSetForegroundArgs)
    notificationManager.notify(id, notification)
  }

  /**
   * This is a wrapper around the new stopForeground method, using the older
   * APIs if it is not available.
   */
  def stopForegroundCompat(id: Int) {
    if (mStopForeground != null) {
      mStopForegroundArgs(0) = boolean2Boolean(x = true)
      try {
        mStopForeground.invoke(this, mStopForegroundArgs: _*)
      }
      catch {
        case e: InvocationTargetException => {
          Log.w(TAG, "Unable to invoke stopForeground", e)
        }
        case e: IllegalAccessException => {
          Log.w(TAG, "Unable to invoke stopForeground", e)
        }
      }
      return
    }
    notificationManager.cancel(id)
    mSetForegroundArgs(0) = boolean2Boolean(x = false)
    invokeMethod(mSetForeground, mSetForegroundArgs)
  }

  val handler: Handler = new Handler {
    override def handleMessage(msg: Message) {
      val ed: SharedPreferences.Editor = settings.edit
      msg.what match {
        case MSG_CONNECT_START =>
          ed.putBoolean("isConnecting", true)
          val pm: PowerManager = getSystemService(Context.POWER_SERVICE).asInstanceOf[PowerManager]
          mWakeLock = pm.newWakeLock(PowerManager.SCREEN_DIM_WAKE_LOCK | PowerManager.ON_AFTER_RELEASE, "GAEProxy")
          mWakeLock.acquire()
        case MSG_CONNECT_FINISH =>
          ed.putBoolean("isConnecting", false)
          if (mWakeLock != null && mWakeLock.isHeld) mWakeLock.release()
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

  val TAG = "ShadowsocksService"
  val BASE = "/data/data/com.github.shadowsocks/"
  val REDSOCKS_CONF = "base {" + " log_debug = off;" + " log_info = off;" + " log = stderr;" + " daemon = on;" + " redirector = iptables;" + "}" + "redsocks {" + " local_ip = 127.0.0.1;" + " local_port = 8123;" + " ip = 127.0.0.1;" + " port = %d;" + " type = socks5;" + "}"
  val SHADOWSOCKS_CONF = "{\"server\": [%s], \"server_port\": %d, \"local_port\": %d, \"password\": %s, \"timeout\": %d}"
  val CMD_IPTABLES_RETURN = " -t nat -A OUTPUT -p tcp -d 0.0.0.0 -j RETURN\n"
  val CMD_IPTABLES_REDIRECT_ADD_SOCKS = " -t nat -A OUTPUT -p tcp " + "-j REDIRECT --to 8123\n"
  val CMD_IPTABLES_DNAT_ADD_SOCKS = " -t nat -A OUTPUT -p tcp " + "-j DNAT --to-destination 127.0.0.1:8123\n"
  val MSG_CONNECT_START: Int = 0
  val MSG_CONNECT_FINISH: Int = 1
  val MSG_CONNECT_SUCCESS: Int = 2
  val MSG_CONNECT_FAIL: Int = 3
  val MSG_HOST_CHANGE: Int = 4
  val MSG_STOP_SELF: Int = 5
  val DNS_PORT: Int = 8153

  val mStartForegroundSignature = Array[Class[_]](classOf[Int], classOf[Notification])
  val mStopForegroundSignature = Array[Class[_]](classOf[Boolean])
  val mSetForegroundSignature = Array[Class[_]](classOf[Boolean])

  var receiver: BroadcastReceiver = null
  var notificationManager: NotificationManager = null
  var mWakeLock: PowerManager#WakeLock = null
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
  var mSetForeground: Method = null
  var mStartForeground: Method = null
  var mStopForeground: Method = null
  var mSetForegroundArgs = new Array[AnyRef](1)
  var mStartForegroundArgs = new Array[AnyRef](2)
  var mStopForegroundArgs = new Array[AnyRef](1)
}