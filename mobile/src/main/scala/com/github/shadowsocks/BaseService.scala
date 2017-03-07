/*******************************************************************************/
/*                                                                             */
/*  Copyright (C) 2017 by Max Lv <max.c.lv@gmail.com>                          */
/*  Copyright (C) 2017 by Mygod Studio <contact-shadowsocks-android@mygod.be>  */
/*                                                                             */
/*  This program is free software: you can redistribute it and/or modify       */
/*  it under the terms of the GNU General Public License as published by       */
/*  the Free Software Foundation, either version 3 of the License, or          */
/*  (at your option) any later version.                                        */
/*                                                                             */
/*  This program is distributed in the hope that it will be useful,            */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of             */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              */
/*  GNU General Public License for more details.                               */
/*                                                                             */
/*  You should have received a copy of the GNU General Public License          */
/*  along with this program. If not, see <http://www.gnu.org/licenses/>.       */
/*                                                                             */
/*******************************************************************************/

package com.github.shadowsocks

import java.io.{File, IOException}
import java.net.InetAddress
import java.util
import java.util.concurrent.TimeUnit
import java.util.{Timer, TimerTask}

import android.app.Service
import android.content.{BroadcastReceiver, Context, Intent, IntentFilter}
import android.os.{Handler, IBinder, RemoteCallbackList}
import android.text.TextUtils
import android.util.Log
import android.widget.Toast
import com.github.shadowsocks.ShadowsocksApplication.app
import com.github.shadowsocks.acl.Acl
import com.github.shadowsocks.aidl.{IShadowsocksService, IShadowsocksServiceCallback}
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.plugin.{PluginConfiguration, PluginManager, PluginOptions}
import com.github.shadowsocks.utils._
import okhttp3.{Dns, FormBody, OkHttpClient, Request}
import org.json.{JSONArray, JSONObject}

import scala.collection.JavaConversions._
import scala.collection.mutable
import scala.collection.mutable.ArrayBuffer
import scala.util.Random

trait BaseService extends Service {

  @volatile private var state = State.STOPPED
  @volatile protected var profile: Profile = _
  @volatile private var plugin: PluginOptions = _
  @volatile protected var pluginPath: String = _

  case class NameNotResolvedException() extends IOException
  case class NullConnectionException() extends NullPointerException

  var timer: Timer = _
  var trafficMonitorThread: TrafficMonitorThread = _

  final val callbacks = new RemoteCallbackList[IShadowsocksServiceCallback]
  private final val bandwidthListeners = new mutable.HashSet[IBinder]() // the binder is the real identifier
  lazy val handler = new Handler(getMainLooper)
  lazy val restartHanlder = new Handler(getMainLooper)

  private val closeReceiver: BroadcastReceiver = (context: Context, _: Intent) => {
    Toast.makeText(context, R.string.stopping, Toast.LENGTH_SHORT).show()
    stopRunner(stopService = true)
  }
  var closeReceiverRegistered: Boolean = _

  val binder = new IShadowsocksService.Stub {
    override def getState: Int = {
      state
    }

    override def getProfileName: String = if (profile == null) null else profile.getName

    override def registerCallback(cb: IShadowsocksServiceCallback): Unit = callbacks.register(cb)

    override def startListeningForBandwidth(cb: IShadowsocksServiceCallback): Unit =
      if (bandwidthListeners.add(cb.asBinder)){
        if (timer == null) {
          timer = new Timer(true)
          timer.schedule(new TimerTask {
            def run(): Unit = if (state == State.CONNECTED && TrafficMonitor.updateRate()) updateTrafficRate()
          }, 1000, 1000)
        }
        TrafficMonitor.updateRate()
        if (state == State.CONNECTED) cb.trafficUpdated(profile.id,
          TrafficMonitor.txRate, TrafficMonitor.rxRate, TrafficMonitor.txTotal, TrafficMonitor.rxTotal)
      }

    override def stopListeningForBandwidth(cb: IShadowsocksServiceCallback): Unit =
      if (bandwidthListeners.remove(cb.asBinder) && bandwidthListeners.isEmpty) {
        timer.cancel()
        timer = null
      }

    override def unregisterCallback(cb: IShadowsocksServiceCallback) {
      stopListeningForBandwidth(cb) // saves an RPC, and safer
      callbacks.unregister(cb)
    }

    override def use(profileId: Int): Unit = synchronized(if (profileId < 0) stopRunner(stopService = true) else {
      val profile = app.profileManager.getProfile(profileId).orNull
      if (profile == null) stopRunner(stopService = true, getString(R.string.profile_empty)) else state match {
        case State.STOPPED => if (checkProfile(profile)) startRunner(profile)
        case State.CONNECTED => if (profileId != BaseService.this.profile.id && checkProfile(profile)) {
          stopRunner(stopService = false)
          startRunner(profile)
        }
        case _ => Log.w(BaseService.this.getClass.getSimpleName, "Illegal state when invoking use: " + state)
      }
    })

    override def useSync(profileId: Int): Unit = use(profileId)
  }

  def checkProfile(profile: Profile): Boolean = if (TextUtils.isEmpty(profile.host) || TextUtils.isEmpty(profile.password)) {
    stopRunner(stopService = true, getString(R.string.proxy_empty))
    false
  } else true

  def connect() {
    if (profile.host == "198.199.101.152") {
      val holder = app.containerHolder
      val container = holder.getContainer
      val url = container.getString("proxy_url")
      val sig = Utils.getSignature(this)

      val client = new OkHttpClient.Builder()
        .dns(hostname => Utils.resolve(hostname, enableIPv6 = false) match {
          case Some(ip) => util.Arrays.asList(InetAddress.getByName(ip))
          case _ => Dns.SYSTEM.lookup(hostname)
        })
        .connectTimeout(10, TimeUnit.SECONDS)
        .writeTimeout(10, TimeUnit.SECONDS)
        .readTimeout(30, TimeUnit.SECONDS)
        .build()
      val requestBody = new FormBody.Builder()
        .add("sig", sig)
        .build()
      val request = new Request.Builder()
        .url(url)
        .post(requestBody)
        .build()

      val proxies = Random.shuffle(client.newCall(request).execute().body.string.split('|').toSeq)
      val proxy = proxies.head.split(':')
      profile.host = proxy(0).trim
      profile.remotePort = proxy(1).trim.toInt
      profile.password = proxy(2).trim
      profile.method = proxy(3).trim
    }

    if (profile.route == Acl.CUSTOM_RULES)  // rationalize custom rules
      Acl.save(Acl.CUSTOM_RULES, new Acl().fromId(Acl.CUSTOM_RULES))

    plugin = new PluginConfiguration(profile.plugin).selectedOptions
    pluginPath = PluginManager.init(plugin)
  }

  def startRunner(profile: Profile) {
    this.profile = profile

    startService(new Intent(this, getClass))
    TrafficMonitor.reset()
    trafficMonitorThread = new TrafficMonitorThread(getApplicationContext)
    trafficMonitorThread.start()

    if (!closeReceiverRegistered) {
      // register close receiver
      val filter = new IntentFilter()
      filter.addAction(Intent.ACTION_SHUTDOWN)
      filter.addAction(Action.CLOSE)
      registerReceiver(closeReceiver, filter)
      closeReceiverRegistered = true
    }

    app.track(getClass.getSimpleName, "start")

    changeState(State.CONNECTING)

    Utils.ThrowableFuture(try connect() catch {
      case _: NameNotResolvedException => stopRunner(stopService = true, getString(R.string.invalid_server))
      case _: NullConnectionException => stopRunner(stopService = true, getString(R.string.reboot_required))
      case exc: Throwable =>
        stopRunner(stopService = true, getString(R.string.service_failed) + ": " + exc.getMessage)
        exc.printStackTrace()
        app.track(exc)
    })
  }

  def stopRunner(stopService: Boolean, msg: String = null) {
    // clean up recevier
    if (closeReceiverRegistered) {
      unregisterReceiver(closeReceiver)
      closeReceiverRegistered = false
    }

    // Make sure update total traffic when stopping the runner
    updateTrafficTotal(TrafficMonitor.txTotal, TrafficMonitor.rxTotal)

    TrafficMonitor.reset()
    if (trafficMonitorThread != null) {
      trafficMonitorThread.stopThread()
      trafficMonitorThread = null
    }

    // change the state
    changeState(State.STOPPED, msg)

    // stop the service if nothing has bound to it
    if (stopService) stopSelf()

    profile = null
  }

  def updateTrafficTotal(tx: Long, rx: Long) {
    val profile = this.profile  // avoid race conditions without locking
    if (profile != null) {
      app.profileManager.getProfile(profile.id) match {
        case Some(p) =>         // default profile may have host, etc. modified
          p.tx += tx
          p.rx += rx
          app.profileManager.updateProfile(p)
          handler.post(() => {
            if (bandwidthListeners.nonEmpty) {
              val n = callbacks.beginBroadcast()
              for (i <- 0 until n) {
                try {
                  val item = callbacks.getBroadcastItem(i)
                  if (bandwidthListeners.contains(item.asBinder)) item.trafficPersisted(p.id)
                } catch {
                  case _: Exception => // Ignore
                }
              }
              callbacks.finishBroadcast()
            }
          })
        case None =>
      }
    }
  }

  def getState: Int = {
    state
  }

  def updateTrafficRate() {
    handler.post(() => {
      if (bandwidthListeners.nonEmpty) {
        val txRate = TrafficMonitor.txRate
        val rxRate = TrafficMonitor.rxRate
        val txTotal = TrafficMonitor.txTotal
        val rxTotal = TrafficMonitor.rxTotal
        val n = callbacks.beginBroadcast()
        for (i <- 0 until n) {
          try {
            val item = callbacks.getBroadcastItem(i)
            if (bandwidthListeners.contains(item.asBinder))
              item.trafficUpdated(profile.id, txRate, rxRate, txTotal, rxTotal)
          } catch {
            case _: Exception => // Ignore
          }
        }
        callbacks.finishBroadcast()
      }
    })
  }


  override def onCreate() {
    super.onCreate()
    app.refreshContainerHolder()
    app.updateAssets()
  }

  // Service of shadowsocks should always be started explicitly
  override def onStartCommand(intent: Intent, flags: Int, startId: Int): Int = Service.START_NOT_STICKY

  protected def changeState(s: Int, msg: String = null) {
    val handler = new Handler(getMainLooper)
    if (state != s || msg != null) {
      if (callbacks.getRegisteredCallbackCount > 0) handler.post(() => {
        val n = callbacks.beginBroadcast()
        for (i <- 0 until n) {
          try {
            callbacks.getBroadcastItem(i).stateChanged(s, binder.getProfileName, msg)
          } catch {
            case _: Exception => // Ignore
          }
        }
        callbacks.finishBroadcast()
      })
      state = s
    }
  }

  def getBlackList: String = {
    val default = getString(R.string.black_list)
    try {
      val container = app.containerHolder.getContainer
      val update = container.getString("black_list_lite")
      val list = if (update == null || update.isEmpty) default else update
      "exclude = " + list + ";"
    } catch {
      case _: Exception => "exclude = " + default + ";"
    }
  }

  protected def buildPluginCommandLine(): ArrayBuffer[String] = {
    val result = ArrayBuffer(pluginPath)
    if (TcpFastOpen.sendEnabled) result += "--fast-open"
    result
  }
  protected final def buildShadowsocksConfig(file: String): String = {
    val config = new JSONObject()
      .put("server", profile.host)
      .put("server_port", profile.remotePort)
      .put("password", profile.password)
      .put("method", profile.method)
    if (pluginPath != null) config
      .put("plugin", Commandline.toString(buildPluginCommandLine()))
      .put("plugin_opts", plugin.toString)
    IOUtils.writeString(new File(getFilesDir, file), config.toString)
    file
  }

  protected final def buildOvertureConfig(file: String): String = {
    val config = new JSONObject()
      .put("BindAddress", ":" + (profile.localPort + 53))
      .put("RedirectIPv6Record", true)
      .put("DomainBase64Decode", true)
      .put("HostsFile", "hosts")
      .put("MinimumTTL", 3600)
      .put("CacheSize", 4096)
    def makeDns(name: String, address: String, edns: Boolean = true) = {
      val dns = new JSONObject()
        .put("Name", name)
        .put("Address", address + ":53")
        .put("Timeout", 6)
        .put("EDNSClientSubnet", new JSONObject().put("Policy", "disable"))
      if (edns) dns
        .put("Protocol", "tcp")
        .put("Socks5Address", "127.0.0.1:" + profile.localPort)
      else dns.put("Protocol", "udp")
      dns
    }
    profile.route match {
      case Acl.BYPASS_CHN | Acl.BYPASS_LAN_CHN | Acl.GFWLIST | Acl.CUSTOM_RULES => config
        .put("PrimaryDNS", new JSONArray(Array(
          makeDns("Primary-1", "119.29.29.29", edns = false),
          makeDns("Primary-2", "114.114.114.114", edns = false)
        )))
        .put("AlternativeDNS", new JSONArray().put(makeDns("Alternative", profile.remoteDns)))
        .put("IPNetworkFile", "china_ip_list.txt")
        .put("DomainFile", "gfwlist.txt")
      case Acl.CHINALIST => config
        .put("PrimaryDNS", new JSONArray().put(makeDns("Primary", "119.29.29.29")))
        .put("AlternativeDNS", new JSONArray().put(makeDns("Alternative", profile.remoteDns)))
      case _ => config
        .put("PrimaryDNS", new JSONArray().put(makeDns("Primary", profile.remoteDns)))
        .put("AlternativeDNS", new JSONArray().put(makeDns("Alternative", "208.67.222.222")))
    }
    IOUtils.writeString(new File(getFilesDir, file), config.toString)
    file
  }
}
