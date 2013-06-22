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
import android.os._
import android.support.v4.app.NotificationCompat
import android.util.Log
import com.google.analytics.tracking.android.EasyTracker
import java.io.BufferedReader
import java.io.FileNotFoundException
import java.io.FileReader
import java.io.IOException
import java.lang.reflect.InvocationTargetException
import java.lang.reflect.Method
import org.apache.http.conn.util.InetAddressUtils
import scala.collection._
import java.util.{TimerTask, Timer}
import android.net.TrafficStats
import android.graphics._
import scala.Some
import scala.concurrent.ops._

case class TrafficStat(tx: Long, rx: Long, timestamp: Long)

object ShadowsocksService {
  def isServiceStarted(context: Context): Boolean = {
    Utils.isServiceStarted("com.github.shadowsocks.ShadowsocksService", context)
  }
}

class ShadowsocksService extends Service {

  val TAG = "ShadowsocksService"
  val BASE = "/data/data/com.github.shadowsocks/"
  val REDSOCKS_CONF = "base {" +
    " log_debug = off;" +
    " log_info = off;" +
    " log = stderr;" +
    " daemon = on;" +
    " redirector = iptables;" +
    "}" +
    "redsocks {" +
    " local_ip = 127.0.0.1;" +
    " local_port = 8123;" +
    " ip = 127.0.0.1;" +
    " port = %d;" +
    " type = socks5;" +
    "}"
  val SHADOWSOCKS_CONF = "{\"server\": [%s], \"server_port\": %d, \"local_port\": %d, \"password\": %s, \"timeout\": %d}"
  val CMD_IPTABLES_RETURN = " -t nat -A OUTPUT -p tcp -d 0.0.0.0 -j RETURN\n"
  val CMD_IPTABLES_REDIRECT_ADD_SOCKS = " -t nat -A OUTPUT -p tcp " + "-j REDIRECT --to 8123\n"
  val CMD_IPTABLES_DNAT_ADD_SOCKS = " -t nat -A OUTPUT -p tcp " +
    "-j DNAT --to-destination 127.0.0.1:8123\n"
  val DNS_PORT = 8153

  val MSG_CONNECT_FINISH = 1
  val MSG_CONNECT_SUCCESS = 2
  val MSG_CONNECT_FAIL = 3
  val MSG_STOP_SELF = 4
  val MSG_VPN_ERROR = 5

  private val mStartForegroundSignature = Array[Class[_]](classOf[Int], classOf[Notification])
  private val mStopForegroundSignature = Array[Class[_]](classOf[Boolean])
  private val mSetForegroundSignature = Array[Class[_]](classOf[Boolean])

  var receiver: BroadcastReceiver = null
  var notificationManager: NotificationManager = null
  var config: Config = null
  var hasRedirectSupport = false
  var apps: Array[ProxiedApp] = null

  private var mSetForeground: Method = null
  private var mStartForeground: Method = null
  private var mStopForeground: Method = null
  private var mSetForegroundArgs = new Array[AnyRef](1)
  private var mStartForegroundArgs = new Array[AnyRef](2)
  private var mStopForegroundArgs = new Array[AnyRef](1)

  private var state = State.INIT
  private var last = new TrafficStat(TrafficStats.getUidTxBytes(getApplicationInfo.uid),
    TrafficStats.getUidRxBytes(getApplicationInfo.uid), java.lang.System.currentTimeMillis())
  private var lastTxRate = 0
  private var lastRxRate = 0
  private val timer = new Timer(true)
  private val TIMER_INTERVAL = 1

  private def changeState(s: Int) {
    if (state != s) {
      state = s
      val intent = new Intent(Action.UPDATE_STATE)
      intent.putExtra(Extra.STATE, state)
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
        case MSG_STOP_SELF =>
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
        .format(config.proxy, config.remotePort, config.localPort, config.sitekey,
        config.encMethod)
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

    changeState(State.CONNECTING)

    config = Extra.get(intent)


    spawn {
      killProcesses()

      var resolved: Boolean = false
      if (!InetAddressUtils.isIPv4Address(config.proxy) &&
        !InetAddressUtils.isIPv6Address(config.proxy)) {
        Utils.resolve(config.proxy, enableIPv6 = true) match {
          case Some(a) =>
            config.proxy = a
            resolved = true
          case None => resolved = false
        }
      } else {
        resolved = true
      }

      hasRedirectSupport = Utils.getHasRedirectSupport

      if (resolved && handleConnection) {
        notifyForegroundAlert(getString(R.string.forward_success),
          getString(R.string.service_running))
        handler.sendEmptyMessageDelayed(MSG_CONNECT_SUCCESS, 500)
      } else {
        notifyAlert(getString(R.string.forward_fail), getString(R.string.service_failed))
        stopSelf()
        handler.sendEmptyMessageDelayed(MSG_CONNECT_FAIL, 500)
      }
      handler.sendEmptyMessageDelayed(MSG_CONNECT_FINISH, 500)
    }
  }

  def startRedsocksDaemon() {
    val conf = REDSOCKS_CONF.format(config.localPort)
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

  /** Called when the activity is first created. */
  def handleConnection: Boolean = {
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
    } catch {
      case e: InvocationTargetException => {
        Log.w(TAG, "Unable to invoke method", e)
      }
      case e: IllegalAccessException => {
        Log.w(TAG, "Unable to invoke method", e)
      }
    }
  }

  def notifyForegroundAlert(title: String, info: String) {
    notifyForegroundAlert(title, info, -1)
  }

  def notifyForegroundAlert(title: String, info: String, rate: Int) {
    val openIntent: Intent = new Intent(this, classOf[Shadowsocks])
    openIntent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP)
    val contentIntent: PendingIntent = PendingIntent.getActivity(this, 0, openIntent, 0)
    val closeIntent: Intent = new Intent(Action.CLOSE)
    val actionIntent: PendingIntent = PendingIntent.getBroadcast(this, 0, closeIntent, 0)
    val builder: NotificationCompat.Builder = new NotificationCompat.Builder(this)

    val icon = getResources.getDrawable(R.drawable.ic_stat_shadowsocks)
    if (rate >= 0) {
      val bitmap = Bitmap.createBitmap(icon.getIntrinsicWidth * 4,
        icon.getIntrinsicHeight * 4, Bitmap.Config.ARGB_8888)
      val r = rate.toString
      val size = bitmap.getHeight / 4
      val canvas = new Canvas(bitmap)
      val paint = new Paint()
      paint.setColor(Color.WHITE)
      paint.setTextSize(size)
      val bounds = new Rect()
      paint.getTextBounds(r, 0, r.length, bounds)
      canvas.drawText(r, (bitmap.getWidth - bounds.width()) / 2,
        bitmap.getHeight - (bitmap.getHeight - bounds.height()) / 2, paint)
      builder.setLargeIcon(bitmap)

      if (rate < 1000) {
        builder.setSmallIcon(R.drawable.ic_stat_speed, rate)
      } else if (rate <= 10000) {
        val mb = rate / 100 - 10 + 1000
        builder.setSmallIcon(R.drawable.ic_stat_speed, mb)
      } else {
        builder.setSmallIcon(R.drawable.ic_stat_speed, 1091)
      }

    } else {
      builder.setSmallIcon(R.drawable.ic_stat_shadowsocks)
    }

    builder
      .setWhen(0)
      .setTicker(title)
      .setContentTitle(getString(R.string.app_name))
      .setContentText(info)
      .setContentIntent(contentIntent)
      .addAction(android.R.drawable.ic_menu_close_clear_cancel, getString(R.string.stop),
      actionIntent)

    startForegroundCompat(1, builder.build)
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

  def onBind(intent: Intent): IBinder = {
    null
  }

  override def onCreate() {
    super.onCreate()
    EasyTracker.getTracker.setStartSession(true)
    EasyTracker.getTracker.sendEvent(TAG, "start", getVersionName, 0L)
    notificationManager = this
      .getSystemService(Context.NOTIFICATION_SERVICE)
      .asInstanceOf[NotificationManager]
    try {
      mStartForeground = getClass.getMethod("startForeground", mStartForegroundSignature: _*)
      mStopForeground = getClass.getMethod("stopForeground", mStopForegroundSignature: _*)
    } catch {
      case e: NoSuchMethodException => {
        mStartForeground = ({
          mStopForeground = null
          mStopForeground
        })
      }
    }
    try {
      mSetForeground = getClass.getMethod("setForeground", mSetForegroundSignature: _*)
    } catch {
      case e: NoSuchMethodException => {
        throw new IllegalStateException(
          "OS doesn't have Service.startForeground OR Service.setForeground!")
      }
    }

    // register close receiver
    val filter = new IntentFilter()
    filter.addAction(Intent.ACTION_SHUTDOWN)
    filter.addAction(Action.CLOSE)
    receiver = new BroadcastReceiver() {
      def onReceive(p1: Context, p2: Intent) {
        stopSelf()
      }
    }
    registerReceiver(receiver, filter)

    // initialize timer
    val task = new TimerTask {
      def run() {
        val now = new TrafficStat(TrafficStats.getUidTxBytes(getApplicationInfo.uid),
          TrafficStats.getUidRxBytes(getApplicationInfo.uid), java.lang.System.currentTimeMillis())
        val txRate = ((now.tx - last.tx) / 1024 / TIMER_INTERVAL).toInt
        val rxRate = ((now.rx - last.rx) / 1024 / TIMER_INTERVAL).toInt
        last = now
        if (lastTxRate == txRate && lastRxRate == rxRate) {
          return
        } else {
          lastTxRate = txRate
          lastRxRate = rxRate
        }
        if (state == State.CONNECTED) {
          notifyForegroundAlert(getString(R.string.forward_success),
            getString(R.string.service_status).format(txRate, rxRate), txRate + rxRate)
        }
      }
    }
    timer.schedule(task, TIMER_INTERVAL*1000, TIMER_INTERVAL*1000)
  }

  /** Called when the activity is closed. */
  override def onDestroy() {

    // clean up context
    changeState(State.STOPPED)
    timer.cancel()
    EasyTracker.getTracker.setStartSession(false)
    EasyTracker.getTracker.sendEvent(TAG, "stop", getVersionName, 0L)
    stopForegroundCompat(1)
    if (receiver != null) {
      unregisterReceiver(receiver)
      receiver = null
    }

    // reset NAT
    killProcesses()

    super.onDestroy()
  }

  def killProcesses() {
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
    if (!InetAddressUtils.isIPv6Address(config.proxy.toUpperCase)) {
      init_sb.append(cmd_bypass.replace("-d 0.0.0.0", "-d " + config.proxy))
    }
    init_sb.append(cmd_bypass.replace("0.0.0.0", "127.0.0.1"))
    init_sb.append(cmd_bypass.replace("-d 0.0.0.0", "--dport " + 53))
    init_sb
      .append(cmd_bypass.replace("-d 0.0.0.0", "-m owner --uid-owner " + getApplicationInfo.uid))
    if (hasRedirectSupport) {
      init_sb
        .append(Utils.getIptables)
        .append(" -t nat -A OUTPUT -p udp --dport 53 -j REDIRECT --to ")
        .append(DNS_PORT)
        .append("\n")
    } else {
      init_sb
        .append(Utils.getIptables)
        .append(" -t nat -A OUTPUT -p udp --dport 53 -j DNAT --to-destination 127.0.0.1:")
        .append(DNS_PORT)
        .append("\n")
    }
    if (config.isGFWList) {
      val chn_list: Array[String] = getResources.getStringArray(R.array.chn_list)
      for (item <- chn_list) {
        init_sb.append(cmd_bypass.replace("0.0.0.0", item))
      }
    }
    if (config.isGlobalProxy || config.isBypassApps) {
      http_sb.append(if (hasRedirectSupport) {
        Utils.getIptables + CMD_IPTABLES_REDIRECT_ADD_SOCKS
      } else {
        Utils.getIptables + CMD_IPTABLES_DNAT_ADD_SOCKS
      })
    }
    if (!config.isGlobalProxy) {
      if (apps == null || apps.length <= 0) {
        apps = AppManager.getProxiedApps(this, config.proxiedAppString)
      }
      val uidSet: mutable.HashSet[Int] = new mutable.HashSet[Int]
      for (app <- apps) {
        if (app.proxied) {
          uidSet.add(app.uid)
        }
      }
      for (uid <- uidSet) {
        if (!config.isBypassApps) {
          http_sb.append((if (hasRedirectSupport) {
            Utils.getIptables + CMD_IPTABLES_REDIRECT_ADD_SOCKS
          } else {
            Utils.getIptables + CMD_IPTABLES_DNAT_ADD_SOCKS
          }).replace("-t nat", "-t nat -m owner --uid-owner " + uid))
        } else {
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
      } catch {
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
}
