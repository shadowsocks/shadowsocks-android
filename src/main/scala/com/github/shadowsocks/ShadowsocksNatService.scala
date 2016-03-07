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
import java.lang.Process
import java.net.{Inet6Address, InetAddress}
import java.util.Locale

import android.content._
import android.content.pm.{PackageInfo, PackageManager}
import android.net.{ConnectivityManager, Network}
import android.os._
import android.util.{Log, SparseArray}
import android.widget.Toast
import com.github.shadowsocks.aidl.Config
import com.github.shadowsocks.utils._

import scala.collection.JavaConversions._
import scala.collection.mutable.ArrayBuffer

class ShadowsocksNatService extends BaseService {

  val TAG = "ShadowsocksNatService"

  val CMD_IPTABLES_RETURN = " -t nat -A OUTPUT -p tcp -d 0.0.0.0 -j RETURN"
  val CMD_IPTABLES_DNAT_ADD_SOCKS = " -t nat -A OUTPUT -p tcp " +
    "-j DNAT --to-destination 127.0.0.1:8123"

  private var notification: ShadowsocksNotification = _
  var closeReceiver: BroadcastReceiver = _
  var connReceiver: BroadcastReceiver = _
  val myUid = android.os.Process.myUid()

  var sslocalProcess: Process = _
  var sstunnelProcess: Process = _
  var redsocksProcess: Process = _
  var pdnsdProcess: Process = _

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
      Console.runRootCommand("ndc resolver flushdefaultif", "ndc resolver flushif wlan0")
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
    connReceiver = (context: Context, intent: Intent) => setupDns()
    registerReceiver(connReceiver, filter)
  }

  def startShadowsocksDaemon() {
    if (config.route != Route.ALL) {
      val acl: Array[Array[String]] = config.route match {
        case Route.BYPASS_LAN => Array(getResources.getStringArray(R.array.private_route))
        case Route.BYPASS_CHN => Array(getResources.getStringArray(R.array.chn_route))
        case Route.BYPASS_LAN_CHN =>
          Array(getResources.getStringArray(R.array.private_route), getResources.getStringArray(R.array.chn_route))
      }
      ConfigUtils.printToFile(new File(getApplicationInfo.dataDir + "/acl.list"))(p => {
        acl.flatten.foreach(p.println)
      })
    }

    val conf = ConfigUtils
      .SHADOWSOCKS.formatLocal(Locale.ENGLISH, config.proxy, config.remotePort, config.localPort,
        config.sitekey, config.encMethod, 600)
    ConfigUtils.printToFile(new File(getApplicationInfo.dataDir + "/ss-local-nat.conf"))(p => {
      p.println(conf)
    })

    val cmd = new ArrayBuffer[String]
    cmd += (getApplicationInfo.dataDir + "/ss-local"
          , "-b" , "127.0.0.1"
          , "-t" , "600"
          , "-P", getApplicationInfo.dataDir
          , "-c" , getApplicationInfo.dataDir + "/ss-local-nat.conf")

    if (config.isAuth) cmd += "-A"

    if (config.route != Route.ALL) {
      cmd += "--acl"
      cmd += (getApplicationInfo.dataDir + "/acl.list")
    }

    if (BuildConfig.DEBUG) Log.d(TAG, cmd.mkString(" "))
    sslocalProcess = new GuardedProcess(cmd)
  }

  def startTunnel() {
    if (config.isUdpDns) {
      val conf = ConfigUtils
        .SHADOWSOCKS.formatLocal(Locale.ENGLISH, config.proxy, config.remotePort, 8153,
          config.sitekey, config.encMethod, 10)
      ConfigUtils.printToFile(new File(getApplicationInfo.dataDir + "/ss-tunnel-nat.conf"))(p => {
        p.println(conf)
      })
      val cmd = new ArrayBuffer[String]
      cmd += (getApplicationInfo.dataDir + "/ss-tunnel"
        , "-u"
        , "-t" , "10"
        , "-b" , "127.0.0.1"
        , "-L" , "8.8.8.8:53"
        , "-P" , getApplicationInfo.dataDir
        , "-c" , getApplicationInfo.dataDir + "/ss-tunnel-nat.conf")

      cmd += ("-l" , "8153")

      if (config.isAuth) cmd += "-A"

      if (BuildConfig.DEBUG) Log.d(TAG, cmd.mkString(" "))

      sstunnelProcess = new GuardedProcess(cmd)

    } else {
      val conf = ConfigUtils
        .SHADOWSOCKS.formatLocal(Locale.ENGLISH, config.proxy, config.remotePort, 8163,
          config.sitekey, config.encMethod, 10)
      ConfigUtils.printToFile(new File(getApplicationInfo.dataDir + "/ss-tunnel-nat.conf"))(p => {
        p.println(conf)
      })
      val cmdBuf = new ArrayBuffer[String]
      cmdBuf += (getApplicationInfo.dataDir + "/ss-tunnel"
        , "-u"
        , "-t" , "10"
        , "-b" , "127.0.0.1"
        , "-l" , "8163"
        , "-L" , "8.8.8.8:53"
        , "-P", getApplicationInfo.dataDir
        , "-c" , getApplicationInfo.dataDir + "/ss-tunnel-nat.conf")

      if (config.isAuth) cmdBuf += "-A"

      if (BuildConfig.DEBUG) Log.d(TAG, cmdBuf.mkString(" "))

      sstunnelProcess = new GuardedProcess(cmdBuf)
    }
  }

  def startDnsDaemon() {

    val conf = if (config.route == Route.BYPASS_CHN || config.route == Route.BYPASS_LAN_CHN) {
      val reject = ConfigUtils.getRejectList(getContext)
      val blackList = ConfigUtils.getBlackList(getContext)
      ConfigUtils.PDNSD_DIRECT.formatLocal(Locale.ENGLISH, getApplicationInfo.dataDir,
        "127.0.0.1", 8153, reject, blackList, 8163, "")
    } else {
      ConfigUtils.PDNSD_LOCAL.formatLocal(Locale.ENGLISH, getApplicationInfo.dataDir,
        "127.0.0.1", 8153, 8163, "")
    }

    ConfigUtils.printToFile(new File(getApplicationInfo.dataDir + "/pdnsd-nat.conf"))(p => {
       p.println(conf)
    })
    val cmd = getApplicationInfo.dataDir + "/pdnsd -c " + getApplicationInfo.dataDir + "/pdnsd-nat.conf"

    if (BuildConfig.DEBUG) Log.d(TAG, cmd)

    pdnsdProcess = new GuardedProcess(cmd.split(" ").toSeq)
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
    val cmd = "%s/redsocks -c %s/redsocks-nat.conf"
      .formatLocal(Locale.ENGLISH, getApplicationInfo.dataDir, getApplicationInfo.dataDir)
    ConfigUtils.printToFile(new File(getApplicationInfo.dataDir + "/redsocks-nat.conf"))(p => {
      p.println(conf)
    })

    if (BuildConfig.DEBUG) Log.d(TAG, cmd)
    redsocksProcess = new GuardedProcess(cmd.split(" ").toSeq)
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
  }

  def killProcesses() {
    if (sslocalProcess != null) {
      sslocalProcess.destroy()
      sslocalProcess = null
    }
    if (sstunnelProcess != null) {
      sstunnelProcess.destroy()
      sstunnelProcess = null
    }
    if (redsocksProcess != null) {
      redsocksProcess.destroy()
      redsocksProcess = null
    }
    if (pdnsdProcess != null) {
      pdnsdProcess.destroy()
      pdnsdProcess = null
    }

    Console.runRootCommand(Utils.getIptables + " -t nat -F OUTPUT")
  }

  def setupIptables() = {
    val init_sb = new ArrayBuffer[String]
    val http_sb = new ArrayBuffer[String]

    init_sb.append("ulimit -n 4096")
    init_sb.append(Utils.getIptables + " -t nat -F OUTPUT")

    val cmd_bypass = Utils.getIptables + CMD_IPTABLES_RETURN
    if (!InetAddress.getByName(config.proxy.toUpperCase).isInstanceOf[Inet6Address]) {
      init_sb.append(cmd_bypass.replace("-p tcp -d 0.0.0.0", "-d " + config.proxy))
    }
    init_sb.append(cmd_bypass.replace("-p tcp -d 0.0.0.0", "-d 127.0.0.1"))
    init_sb.append(cmd_bypass.replace("-p tcp -d 0.0.0.0", "-m owner --uid-owner " + myUid))
    init_sb.append(cmd_bypass.replace("-d 0.0.0.0", "--dport 53"))

    init_sb.append(Utils.getIptables
      + " -t nat -A OUTPUT -p udp --dport 53 -j DNAT --to-destination 127.0.0.1:8153")

    if (!config.isProxyApps || config.isBypassApps) {
      http_sb.append(Utils.getIptables + CMD_IPTABLES_DNAT_ADD_SOCKS)
    }
    if (config.isProxyApps) {
      val uidMap = getPackageManager.getInstalledApplications(0).map(ai => ai.packageName -> ai.uid).toMap
      for (pn <- config.proxiedAppString.split('\n')) uidMap.get(pn) match {
        case Some(uid) =>
          if (!config.isBypassApps) {
            http_sb.append((Utils.getIptables + CMD_IPTABLES_DNAT_ADD_SOCKS)
              .replace("-t nat", "-t nat -m owner --uid-owner " + uid))
          } else {
            init_sb.append(cmd_bypass.replace("-d 0.0.0.0", "-m owner --uid-owner " + uid))
          }
        case _ => // probably removed package, ignore
      }
    }
    Console.runRootCommand((init_sb ++ http_sb).toArray)
  }

  override def startRunner(config: Config) {
    if (!Console.isRoot) {
      changeState(State.STOPPED, getString(R.string.nat_no_root))
      return
    }
    super.startRunner(config)

    // register close receiver
    val filter = new IntentFilter()
    filter.addAction(Intent.ACTION_SHUTDOWN)
    filter.addAction(Action.CLOSE)
    closeReceiver = (context: Context, intent: Intent) => {
      Toast.makeText(context, R.string.stopping, Toast.LENGTH_SHORT).show()
      stopRunner()
    }
    registerReceiver(closeReceiver, filter)

    ShadowsocksApplication.track(TAG, "start")
    
    changeState(State.CONNECTING)

    ThrowableFuture {
      if (config.proxy == "198.199.101.152") {
        val holder = ShadowsocksApplication.containerHolder
        try {
          this.config = ConfigUtils.getPublicConfig(getBaseContext, holder.getContainer, config)
        } catch {
          case ex: Exception =>
            changeState(State.STOPPED, getString(R.string.service_failed))
            stopRunner()
            this.config = null
        }
      }

      if (this.config != null) {

        // Clean up
        killProcesses()

        var resolved: Boolean = false
        if (!Utils.isNumeric(config.proxy)) {
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

          changeState(State.CONNECTED)
          notification = new ShadowsocksNotification(this, config.profileName, true)
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

    if (notification != null) notification.destroy()

    ShadowsocksApplication.track(TAG, "stop")

    // reset NAT
    killProcesses()

    super.stopRunner()
  }

  override def getTag = TAG
  override def getServiceMode = Mode.NAT
  override def getContext = getBaseContext
}
