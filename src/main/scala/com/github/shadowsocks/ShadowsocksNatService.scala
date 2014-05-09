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
import com.google.analytics.tracking.android.{Fields, MapBuilder, EasyTracker}
import java.lang.reflect.InvocationTargetException
import java.lang.reflect.Method
import org.apache.http.conn.util.InetAddressUtils
import scala.collection._
import java.util.{TimerTask, Timer}
import android.net.TrafficStats
import scala.concurrent.ops._
import com.github.shadowsocks.utils._
import scala.Some
import android.graphics.Color
import com.github.shadowsocks.aidl.Config
import scala.collection.mutable.ArrayBuffer
import java.io.File

case class TrafficStat(tx: Long, rx: Long, timestamp: Long)

class ShadowsocksNatService extends Service with BaseService {

  val TAG = "ShadowsocksNatService"

  val CMD_IPTABLES_RETURN = " -t nat -A OUTPUT -p tcp -d 0.0.0.0 -j RETURN"
  val CMD_IPTABLES_REDIRECT_ADD_SOCKS = " -t nat -A OUTPUT -p tcp " + "-j REDIRECT --to 8123"
  val CMD_IPTABLES_DNAT_ADD_SOCKS = " -t nat -A OUTPUT -p tcp " +
    "-j DNAT --to-destination 127.0.0.1:8123"
  val DNS_PORT = 8153

  private val mStartForegroundSignature = Array[Class[_]](classOf[Int], classOf[Notification])
  private val mStopForegroundSignature = Array[Class[_]](classOf[Boolean])
  private val mSetForegroundSignature = Array[Class[_]](classOf[Boolean])

  var receiver: BroadcastReceiver = null
  var notificationManager: NotificationManager = null
  var config: Config = null
  var hasRedirectSupport = false
  var apps: Array[ProxiedApp] = null
  val uid = Process.myUid()

  private var mSetForeground: Method = null
  private var mStartForeground: Method = null
  private var mStopForeground: Method = null
  private var mSetForegroundArgs = new Array[AnyRef](1)
  private var mStartForegroundArgs = new Array[AnyRef](2)
  private var mStopForegroundArgs = new Array[AnyRef](1)

  private var last: TrafficStat = null
  private var lastTxRate = 0
  private var lastRxRate = 0
  private var timer: Timer = null
  private val TIMER_INTERVAL = 2

  val handler: Handler = new Handler()

  def startShadowsocksDaemon() {
    val cmd = (Path.BASE +
      "ss-local -b 127.0.0.1 -s \"%s\" -p \"%d\" -l \"%d\" -k \"%s\" -m \"%s\" -f " +
      Path.BASE + "ss-local.pid")
      .format(config.proxy, config.remotePort, config.localPort, config.sitekey, config.encMethod)
    if (BuildConfig.DEBUG) Log.d(TAG, cmd)
    System.exec(cmd)
  }

  def startDnsDaemon() {
    val cmd = if (config.isUdpDns) {
      (Path.BASE +
        "ss-tunnel -b 127.0.0.1 -s \"%s\" -p \"%d\" -l \"%d\" -k \"%s\" -m \"%s\" -L 8.8.8.8:53 -u -f " +
        Path.BASE + "ss-tunnel.pid")
        .format(config.proxy, config.remotePort, 8153, config.sitekey, config.encMethod)
    } else {
      val conf = {
        if (config.isGFWList)
          ConfigUtils.PDNSD_BYPASS.format("127.0.0.1", getString(R.string.exclude))
        else
          ConfigUtils.PDNSD.format("127.0.0.1")
      }
      ConfigUtils.printToFile(new File(Path.BASE + "pdnsd.conf"))(p => {
         p.println(conf)
      })
      Path.BASE + "pdnsd -c " + Path.BASE + "pdnsd.conf"
    }
    if (BuildConfig.DEBUG) Log.d(TAG, cmd)
    System.exec(cmd)
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

  def startRedsocksDaemon() {
    val conf = ConfigUtils.REDSOCKS.format(config.localPort)
    val cmd = "%sredsocks -p %sredsocks.pid -c %sredsocks.conf"
      .format(Path.BASE, Path.BASE, Path.BASE)
    ConfigUtils.printToFile(new File(Path.BASE + "redsocks.conf"))(p => {
      p.println(conf)
    })
    Console.runRootCommand(cmd)
  }

  /** Called when the activity is first created. */
  def handleConnection: Boolean = {

    startShadowsocksDaemon()
    startDnsDaemon()
    startRedsocksDaemon()
    setupIptables
    flushDNS()

    true
  }

  def invokeMethod(method: Method, args: Array[AnyRef]) {
    try {
      method.invoke(this, mStartForegroundArgs: _*)
    } catch {
      case e: InvocationTargetException =>
        Log.w(TAG, "Unable to invoke method", e)
      case e: IllegalAccessException =>
        Log.w(TAG, "Unable to invoke method", e)
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
      val bitmap = Utils
        .getBitmap(rate.toString, icon.getIntrinsicWidth * 4, icon.getIntrinsicHeight * 4,
        Color.TRANSPARENT)
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
    if (Action.SERVICE == intent.getAction) {
      binder
    } else {
      null
    }
  }

  override def onCreate() {
    super.onCreate()

    ConfigUtils.refresh(this)

    notificationManager = this
      .getSystemService(Context.NOTIFICATION_SERVICE)
      .asInstanceOf[NotificationManager]
    try {
      mStartForeground = getClass.getMethod("startForeground", mStartForegroundSignature: _*)
      mStopForeground = getClass.getMethod("stopForeground", mStopForegroundSignature: _*)
    } catch {
      case e: NoSuchMethodException =>
        mStartForeground = {
          mStopForeground = null
          mStopForeground
        }
    }
    try {
      mSetForeground = getClass.getMethod("setForeground", mSetForegroundSignature: _*)
    } catch {
      case e: NoSuchMethodException =>
        throw new IllegalStateException(
          "OS doesn't have Service.startForeground OR Service.setForeground!")
    }
  }

  def killProcesses() {
    Console.runRootCommand(Utils.getIptables + " -t nat -F OUTPUT")

    val ab = new ArrayBuffer[String]

    ab.append("kill -9 `cat " + Path.BASE +"redsocks.pid`")
    ab.append("killall -9 redsocks")

    Console.runRootCommand(ab.toArray)

    ab.clear()

    ab.append("kill -9 `cat " + Path.BASE + "ss-local.pid`")
    ab.append("killall -9 ss-local")
    ab.append("kill -9 `cat " + Path.BASE + "ss-tunnel.pid`")
    ab.append("killall -9 ss-tunnel")
    ab.append("kill -15 `cat " + Path.BASE + "pdnsd.pid`")
    ab.append("killall -15 pdnsd")

    Console.runRootCommand(ab.toArray)
  }

  override def onStartCommand(intent: Intent, flags: Int, startId: Int): Int = {
    Service.START_STICKY
  }

  def flushDNS() {
    Console.runRootCommand(Array("ndc resolver flushdefaultif", "ndc resolver flushif wlan0"))
  }

  def setupIptables: Boolean = {
    val init_sb = new ArrayBuffer[String]
    val http_sb = new ArrayBuffer[String]
    init_sb.append(Utils.getIptables + " -t nat -F OUTPUT")
    val cmd_bypass = Utils.getIptables + CMD_IPTABLES_RETURN
    if (!InetAddressUtils.isIPv6Address(config.proxy.toUpperCase)) {
      init_sb.append(cmd_bypass.replace("-d 0.0.0.0", "-d " + config.proxy))
    }
    init_sb.append(cmd_bypass.replace("0.0.0.0", "127.0.0.1"))
    if (config.isGFWList) {
      // Bypass DNS in China
      init_sb.append(cmd_bypass.replace("-p tcp -d 0.0.0.0", "-d 114.114.114.114"))
      init_sb.append(cmd_bypass.replace("-p tcp -d 0.0.0.0", "-d 114.114.115.115"))
    }
    if (hasRedirectSupport) {
      init_sb
        .append(Utils.getIptables + " -t nat -A OUTPUT -p udp --dport 53 -j REDIRECT --to "
        + DNS_PORT)
    } else {
      init_sb
        .append(Utils.getIptables
        + " -t nat -A OUTPUT -p udp --dport 53 -j DNAT --to-destination 127.0.0.1:"
        + DNS_PORT)
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
    Console.runRootCommand(init_sb.toArray)
    Console.runRootCommand(http_sb.toArray)
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
        case e: InvocationTargetException =>
          Log.w(TAG, "Unable to invoke stopForeground", e)
        case e: IllegalAccessException =>
          Log.w(TAG, "Unable to invoke stopForeground", e)
      }
      return
    }
    notificationManager.cancel(id)
    mSetForegroundArgs(0) = boolean2Boolean(x = false)
    invokeMethod(mSetForeground, mSetForegroundArgs)
  }

  override def startRunner(c: Config) {

    config = c

    // register close receiver
    val filter = new IntentFilter()
    filter.addAction(Intent.ACTION_SHUTDOWN)
    filter.addAction(Action.CLOSE)
    receiver = new BroadcastReceiver() {
      def onReceive(p1: Context, p2: Intent) {
        spawn {
          stopRunner()
        }
      }
    }
    registerReceiver(receiver, filter)

    // start tracker
    EasyTracker
      .getInstance(this)
      .send(MapBuilder
      .createEvent(TAG, "start", getVersionName, 0L)
      .set(Fields.SESSION_CONTROL, "start")
      .build())

    changeState(State.CONNECTING)

    if (config.isTrafficStat) {
      // initialize timer
      val task = new TimerTask {
        def run() {
          val pm = getSystemService(Context.POWER_SERVICE).asInstanceOf[PowerManager]
          val now = new
              TrafficStat(TrafficStats.getUidTxBytes(uid), TrafficStats.getUidRxBytes(uid),
                java.lang.System.currentTimeMillis())
          val txRate = ((now.tx - last.tx) / 1024 / TIMER_INTERVAL).toInt
          val rxRate = ((now.rx - last.rx) / 1024 / TIMER_INTERVAL).toInt
          last = now
          if (lastTxRate == txRate && lastRxRate == rxRate) {
            return
          } else {
            lastTxRate = txRate
            lastRxRate = rxRate
          }
          if ((pm.isScreenOn && state == State.CONNECTED) || (txRate == 0 && rxRate == 0)) {
            notifyForegroundAlert(getString(R.string.forward_success),
              getString(R.string.service_status).format(math.max(txRate, rxRate)),
              math.max(txRate, rxRate))
          }
        }
      }
      last = new TrafficStat(TrafficStats.getUidTxBytes(uid), TrafficStats.getUidRxBytes(uid),
        java.lang.System.currentTimeMillis())
      timer = new Timer(true)
      timer.schedule(task, TIMER_INTERVAL * 1000, TIMER_INTERVAL * 1000)
    }

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
          getString(R.string.service_running).format(config.profileName))
        changeState(State.CONNECTED)
      } else {
        notifyAlert(getString(R.string.forward_fail), getString(R.string.service_failed))
        stopRunner()
        changeState(State.STOPPED)
      }
    }
  }

  override def stopRunner() {

    // change the state
    changeState(State.STOPPED)

    // stop the tracker
    EasyTracker
      .getInstance(this)
      .send(MapBuilder
      .createEvent(TAG, "stop", getVersionName, 0L)
      .set(Fields.SESSION_CONTROL, "stop")
      .build())

    // reset timer
    if (timer != null) {
      timer.cancel()
      timer = null
    }

    // clean up context
    stopForegroundCompat(1)
    if (receiver != null) {
      unregisterReceiver(receiver)
      receiver = null
    }

    // reset NAT
    killProcesses()

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
  override def getServiceMode = Mode.NAT
  override def getContext = getBaseContext
}
