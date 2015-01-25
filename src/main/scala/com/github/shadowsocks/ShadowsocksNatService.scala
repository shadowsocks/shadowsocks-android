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
import java.lang.reflect.{InvocationTargetException, Method}
import java.util.Locale

import android.app._
import android.content._
import android.content.pm.{PackageInfo, PackageManager}
import android.net.{Network, ConnectivityManager}
import android.os._
import android.support.v4.app.NotificationCompat
import android.util.{SparseArray, Log}
import android.widget.Toast
import com.github.shadowsocks.aidl.Config
import com.github.shadowsocks.utils._
import com.google.android.gms.analytics.HitBuilders
import org.apache.http.conn.util.InetAddressUtils

import scala.collection._
import scala.collection.mutable.ArrayBuffer
import scala.concurrent.ops._

class ShadowsocksNatService extends Service with BaseService {

  val TAG = "ShadowsocksNatService"

  val CMD_IPTABLES_RETURN = " -t nat -A OUTPUT -p tcp -d 0.0.0.0 -j RETURN"
  val CMD_IPTABLES_DNAT_ADD_SOCKS = " -t nat -A OUTPUT -p tcp " +
    "-j DNAT --to-destination 127.0.0.1:8123"

  private val mStartForegroundSignature = Array[Class[_]](classOf[Int], classOf[Notification])
  private val mStopForegroundSignature = Array[Class[_]](classOf[Boolean])
  private val mSetForegroundSignature = Array[Class[_]](classOf[Boolean])
  private val mSetForegroundArgs = new Array[AnyRef](1)
  private val mStartForegroundArgs = new Array[AnyRef](2)
  private val mStopForegroundArgs = new Array[AnyRef](1)

  var lockReceiver: BroadcastReceiver = null
  var closeReceiver: BroadcastReceiver = null
  var connReceiver: BroadcastReceiver = null
  var notificationManager: NotificationManager = null
  var config: Config = null
  var apps: Array[ProxiedApp] = null
  val myUid = Process.myUid()

  private var mSetForeground: Method = null
  private var mStartForeground: Method = null
  private var mStopForeground: Method = null

  private lazy val application = getApplication.asInstanceOf[ShadowsocksApplication]

  private val dnsAddressCache = new SparseArray[String]

  def getNetId(network: Network): Int = {
    network.getClass.getDeclaredField("netId").get(network).asInstanceOf[Int]
  }

  def restoreDnsForAllNetwork() {
    val manager = getSystemService(Context.CONNECTIVITY_SERVICE).asInstanceOf[ConnectivityManager]
    val networks = manager.getAllNetworks
    val cmdBuf = new ArrayBuffer[String]()
    networks.foreach(network => {
      val netId = getNetId(network)
      val oldDns = dnsAddressCache.get(netId)
      if (oldDns != null) {
        cmdBuf.append("ndc resolver setnetdns %d \"\" %s".formatLocal(Locale.ENGLISH, netId, oldDns))
        dnsAddressCache.remove(netId)
      }
    })
    if (cmdBuf.nonEmpty) Console.runRootCommand(cmdBuf.toArray)
  }

  def setDnsForAllNetwork(dns: String) {
    val manager = getSystemService(Context.CONNECTIVITY_SERVICE).asInstanceOf[ConnectivityManager]
    val networks = manager.getAllNetworks
    if (networks == null) return

    val cmdBuf = new ArrayBuffer[String]()
    networks.foreach(network => {
      val networkInfo = manager.getNetworkInfo(network)
      if (networkInfo == null) return
      if (networkInfo.isConnected) {
        val netId = getNetId(network)
        val curDnsList = manager.getLinkProperties(network).getDnsServers
        if (curDnsList != null) {
          import scala.collection.JavaConverters._
          val curDns = curDnsList.asScala.map(ip => ip.getHostAddress).mkString(" ")
          if (curDns != dns) {
            dnsAddressCache.put(netId, curDns)
            cmdBuf.append("ndc resolver setnetdns %d \"\" %s".formatLocal(Locale.ENGLISH, netId, dns))
          }
        }
      }
    })
    if (cmdBuf.nonEmpty) Console.runRootCommand(cmdBuf.toArray)
  }

  def setupDns() {
    setDnsForAllNetwork("127.0.0.1")
  }

  def resetDns() = {
    restoreDnsForAllNetwork()
  }

  def flushDns() {
    if (Utils.isLollipopOrAbove) {
      val manager = getSystemService(Context.CONNECTIVITY_SERVICE).asInstanceOf[ConnectivityManager]
      val networks = manager.getAllNetworks
      val cmdBuf = new ArrayBuffer[String]()
      networks.foreach(network => {
        val networkInfo = manager.getNetworkInfo(network)
        if (networkInfo.isAvailable) {
          val netId = network.getClass.getDeclaredField("netId").get(network).asInstanceOf[Int]
          cmdBuf.append("ndc resolver flushnet %d".formatLocal(Locale.ENGLISH, netId))
        }
      })
      Console.runRootCommand(cmdBuf.toArray)
    } else {
      Console.runRootCommand(Array("ndc resolver flushdefaultif", "ndc resolver flushif wlan0"))
    }
  }


  def destroyConnectionReceiver() {
    if (connReceiver != null) {
      unregisterReceiver(connReceiver)
      connReceiver = null
    }
    resetDns()
  }

  def initConnectionReceiver() {
    val filter = new IntentFilter(ConnectivityManager.CONNECTIVITY_ACTION)
    connReceiver = new BroadcastReceiver {
      override def onReceive(context: Context, intent: Intent) = {
        setupDns()
      }
    }
    registerReceiver(connReceiver, filter)
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
        config.sitekey, config.encMethod, 10)
    ConfigUtils.printToFile(new File(Path.BASE + "ss-local-nat.conf"))(p => {
      p.println(conf)
    })

    val cmd = new ArrayBuffer[String]
    cmd += (Path.BASE + "ss-local"
          , "-b" , "127.0.0.1"
          , "-t" , "600"
          , "-c" , Path.BASE + "ss-local-nat.conf"
          , "-f" , Path.BASE + "ss-local-nat.pid")

    if (config.route != Route.ALL) {
      cmd += "--acl"
      cmd += (Path.BASE + "acl.list")
    }

    if (BuildConfig.DEBUG) Log.d(TAG, cmd.mkString(" "))
    Console.runCommand(cmd.mkString(" "))
  }

  def startTunnel() {
    if (config.isUdpDns) {
      val conf = ConfigUtils
        .SHADOWSOCKS.formatLocal(Locale.ENGLISH, config.proxy, config.remotePort, 8153,
          config.sitekey, config.encMethod, 10)
      ConfigUtils.printToFile(new File(Path.BASE + "ss-tunnel-nat.conf"))(p => {
        p.println(conf)
      })
      val cmd = new ArrayBuffer[String]
      cmd += (Path.BASE + "ss-tunnel"
        , "-u"
        , "-t" , "10"
        , "-b" , "127.0.0.1"
        , "-L" , "8.8.8.8:53"
        , "-c" , Path.BASE + "ss-tunnel-nat.conf"
        , "-f" , Path.BASE + "ss-tunnel-nat.pid")

      cmd += ("-l" , "8153")

      if (BuildConfig.DEBUG) Log.d(TAG, cmd.mkString(" "))

      Console.runCommand(cmd.mkString(" "))

    } else {
      val conf = ConfigUtils
        .SHADOWSOCKS.formatLocal(Locale.ENGLISH, config.proxy, config.remotePort, 8163,
          config.sitekey, config.encMethod, 10)
      ConfigUtils.printToFile(new File(Path.BASE + "ss-tunnel-nat.conf"))(p => {
        p.println(conf)
      })
      val cmdBuf = new ArrayBuffer[String]
      cmdBuf += (Path.BASE + "ss-tunnel"
        , "-u"
        , "-t" , "10"
        , "-b" , "127.0.0.1"
        , "-l" , "8163"
        , "-L" , "8.8.8.8:53"
        , "-c" , Path.BASE + "ss-tunnel-nat.conf"
        , "-f" , Path.BASE + "ss-tunnel-nat.pid")

      if (BuildConfig.DEBUG) Log.d(TAG, cmdBuf.mkString(" "))
      Console.runCommand(cmdBuf.mkString(" "))
    }
  }

  def startDnsDaemon() {

    val conf = if (config.route == Route.BYPASS_CHN) {
      val reject = ConfigUtils.getRejectList(getContext, application)
      val blackList = ConfigUtils.getBlackList(getContext, application)
      ConfigUtils.PDNSD_DIRECT.formatLocal(Locale.ENGLISH, "127.0.0.1", 8153,
        Path.BASE + "pdnsd-nat.pid", reject, blackList, 8163)
    } else {
      ConfigUtils.PDNSD_LOCAL.formatLocal(Locale.ENGLISH, "127.0.0.1", 8153,
        Path.BASE + "pdnsd-nat.pid", 8163)
    }

    ConfigUtils.printToFile(new File(Path.BASE + "pdnsd-nat.conf"))(p => {
       p.println(conf)
    })
    val cmd = Path.BASE + "pdnsd -c " + Path.BASE + "pdnsd-nat.conf"

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

  def startRedsocksDaemon() {
    val conf = ConfigUtils.REDSOCKS.formatLocal(Locale.ENGLISH, config.localPort)
    val cmd = Path.BASE + "redsocks -p %sredsocks-nat.pid -c %sredsocks-nat.conf"
      .formatLocal(Locale.ENGLISH, Path.BASE, Path.BASE)
    ConfigUtils.printToFile(new File(Path.BASE + "redsocks-nat.conf"))(p => {
      p.println(conf)
    })

    if (BuildConfig.DEBUG) Log.d(TAG, cmd)
    Console.runCommand(cmd)
  }

  /** Called when the activity is first created. */
  def handleConnection: Boolean = {

    startTunnel()
    if (!config.isUdpDns) startDnsDaemon()
    startRedsocksDaemon()
    startShadowsocksDaemon()
    setupIptables()

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

  def notifyForegroundAlert(title: String, info: String, visible: Boolean) {
    val openIntent = new Intent(this, classOf[Shadowsocks])
    openIntent.setFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT)
    val contentIntent = PendingIntent.getActivity(this, 0, openIntent, 0)
    val closeIntent = new Intent(Action.CLOSE)
    val actionIntent = PendingIntent.getBroadcast(this, 0, closeIntent, 0)
    val builder = new NotificationCompat.Builder(this)

    builder
      .setWhen(0)
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

    startForegroundCompat(1, builder.build)
  }

  def onBind(intent: Intent): IBinder = {
    Log.d(TAG, "onBind")
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
    val cmd = new ArrayBuffer[String]()

    for (task <- Array("ss-local", "ss-tunnel", "pdnsd", "redsocks")) {
      cmd.append("chmod 666 %s%s-nat.pid".formatLocal(Locale.ENGLISH, Path.BASE, task))
    }
    Console.runRootCommand(cmd.toArray)
    cmd.clear()

    for (task <- Array("ss-local", "ss-tunnel", "pdnsd", "redsocks")) {
      try {
        val pid = scala.io.Source.fromFile(Path.BASE + task + "-nat.pid").mkString.trim.toInt
        cmd.append("kill -9 %d".formatLocal(Locale.ENGLISH, pid))
        Process.killProcess(pid)
      } catch {
        case e: Throwable => Log.e(TAG, "unable to kill " + task)
      }
      cmd.append("rm -f %s%s-nat.pid".formatLocal(Locale.ENGLISH, Path.BASE, task))
      cmd.append("rm -f %s%s-nat.conf".formatLocal(Locale.ENGLISH, Path.BASE, task))
    }

    Console.runRootCommand(cmd.toArray)
    Console.runRootCommand(Utils.getIptables + " -t nat -F OUTPUT")
  }

  def setupIptables() = {
    val init_sb = new ArrayBuffer[String]
    val http_sb = new ArrayBuffer[String]

    init_sb.append("ulimit -n 4096")
    init_sb.append(Utils.getIptables + " -t nat -F OUTPUT")

    val cmd_bypass = Utils.getIptables + CMD_IPTABLES_RETURN
    if (!InetAddressUtils.isIPv6Address(config.proxy.toUpperCase)) {
      init_sb.append(cmd_bypass.replace("-p tcp -d 0.0.0.0", "-d " + config.proxy))
    }
    init_sb.append(cmd_bypass.replace("-p tcp -d 0.0.0.0", "-d 127.0.0.1"))
    init_sb.append(cmd_bypass.replace("-p tcp -d 0.0.0.0", "-m owner --uid-owner " + myUid))
    init_sb.append(cmd_bypass.replace("-d 0.0.0.0", "--dport 53"))

    init_sb.append(Utils.getIptables
      + " -t nat -A OUTPUT -p udp --dport 53 -j DNAT --to-destination 127.0.0.1:" + 8153)

    if (config.isGlobalProxy || config.isBypassApps) {
      http_sb.append(Utils.getIptables + CMD_IPTABLES_DNAT_ADD_SOCKS)
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
          http_sb.append((Utils.getIptables + CMD_IPTABLES_DNAT_ADD_SOCKS).replace("-t nat", "-t nat -m owner --uid-owner " + uid))
        } else {
          init_sb.append(cmd_bypass.replace("-d 0.0.0.0", "-m owner --uid-owner " + uid))
        }
      }
    }
    Console.runRootCommand(init_sb.toArray)
    Console.runRootCommand(http_sb.toArray)
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
    closeReceiver = new BroadcastReceiver() {
      def onReceive(context: Context, intent: Intent) {
        Toast.makeText(context, R.string.stopping, Toast.LENGTH_SHORT).show()
        stopRunner()
      }
    }
    registerReceiver(closeReceiver, filter)

    if (Utils.isLollipopOrAbove) {
      val screenFilter = new IntentFilter()
      screenFilter.addAction(Intent.ACTION_SCREEN_ON)
      screenFilter.addAction(Intent.ACTION_SCREEN_OFF)
      screenFilter.addAction(Intent.ACTION_USER_PRESENT)
      lockReceiver = new BroadcastReceiver() {
        def onReceive(context: Context, intent: Intent) {
          if (getState == State.CONNECTED) {
            val action = intent.getAction
            if (action == Intent.ACTION_SCREEN_OFF) {
              notifyForegroundAlert(getString(R.string.forward_success),
                getString(R.string.service_running).formatLocal(Locale.ENGLISH, config.profileName), false)
            } else if (action == Intent.ACTION_SCREEN_ON) {
              val keyGuard = getSystemService(Context.KEYGUARD_SERVICE).asInstanceOf[KeyguardManager]
              if (!keyGuard.inKeyguardRestrictedInputMode) {
                notifyForegroundAlert(getString(R.string.forward_success),
                  getString(R.string.service_running).formatLocal(Locale.ENGLISH, config.profileName), true)
            }
            } else if (action == Intent.ACTION_USER_PRESENT) {
              notifyForegroundAlert(getString(R.string.forward_success),
                getString(R.string.service_running).formatLocal(Locale.ENGLISH, config.profileName), true)
            }
            }
          }
        }
        registerReceiver(lockReceiver, screenFilter)
    }

    // send event
    application.tracker.send(new HitBuilders.EventBuilder()
      .setCategory(TAG)
      .setAction("start")
      .setLabel(getVersionName)
      .build())

    changeState(State.CONNECTING)

    spawn {

      if (config.proxy == "198.199.101.152") {
        val holder = application.containerHolder
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

        // Clean up
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

        if (resolved && handleConnection) {

          // Set DNS
          flushDns()

          notifyForegroundAlert(getString(R.string.forward_success),
            getString(R.string.service_running).formatLocal(Locale.ENGLISH, config.profileName), true)
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

    // clean up recevier
    if (closeReceiver != null) {
      unregisterReceiver(closeReceiver)
      closeReceiver = null
    }

    if (Utils.isLollipopOrAbove) {
      if (lockReceiver != null) {
        unregisterReceiver(lockReceiver)
        lockReceiver = null
      }
    }

    // send event
    application.tracker.send(new HitBuilders.EventBuilder()
      .setCategory(TAG)
      .setAction("stop")
      .setLabel(getVersionName)
      .build())

    // reset NAT
    killProcesses()

    // stop the service if no callback registered
    if (getCallbackCount == 0) {
      stopSelf()
    }

    stopForegroundCompat(1)

    // change the state
    changeState(State.STOPPED)
  }

  override def stopBackgroundService() {
    stopSelf()
  }

  override def getTag = TAG
  override def getServiceMode = Mode.NAT
  override def getContext = getBaseContext
}
