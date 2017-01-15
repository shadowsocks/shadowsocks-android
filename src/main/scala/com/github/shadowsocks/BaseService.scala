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

import java.io.IOException
import java.net.InetAddress
import java.util
import java.util.concurrent.TimeUnit
import java.util.{Timer, TimerTask}

import android.app.Service
import android.content.{BroadcastReceiver, Context, Intent, IntentFilter}
import android.os.{Handler, RemoteCallbackList}
import android.text.TextUtils
import android.util.Log
import android.widget.Toast
import com.github.shadowsocks.ShadowsocksApplication.app
import com.github.shadowsocks.aidl.{IShadowsocksService, IShadowsocksServiceCallback}
import com.github.shadowsocks.utils._
import com.github.shadowsocks.ShadowsocksApplication.app
import com.github.shadowsocks.database.Profile
import okhttp3.{Dns, FormBody, OkHttpClient, Request}


import scala.util.Random

trait BaseService extends Service {

  @volatile private var state = State.STOPPED
  @volatile protected var profile: Profile = _

  case class NameNotResolvedException() extends IOException
  case class KcpcliParseException(cause: Throwable) extends Exception(cause)
  case class NullConnectionException() extends NullPointerException

  var timer: Timer = _
  var trafficMonitorThread: TrafficMonitorThread = _

  final val callbacks = new RemoteCallbackList[IShadowsocksServiceCallback]
  var callbacksCount: Int = _
  lazy val handler = new Handler(getMainLooper)
  lazy val restartHanlder = new Handler(getMainLooper)
  lazy val protectPath = getApplicationInfo.dataDir + "/protect_path"

  private val closeReceiver: BroadcastReceiver = (context: Context, intent: Intent) => {
    Toast.makeText(context, R.string.stopping, Toast.LENGTH_SHORT).show()
    stopRunner(true)
  }
  var closeReceiverRegistered: Boolean = _

  val binder = new IShadowsocksService.Stub {
    override def getState: Int = {
      state
    }

    override def getProfileName: String = if (profile == null) null else profile.name

    override def unregisterCallback(cb: IShadowsocksServiceCallback) {
      if (cb != null && callbacks.unregister(cb)) {
        callbacksCount -= 1
        if (callbacksCount == 0 && timer != null) {
          timer.cancel()
          timer = null
        }
      }
    }

    override def registerCallback(cb: IShadowsocksServiceCallback) {
      if (cb != null && callbacks.register(cb)) {
        callbacksCount += 1
        if (callbacksCount != 0 && timer == null) {
          val task = new TimerTask {
            def run {
              if (TrafficMonitor.updateRate()) updateTrafficRate()
            }
          }
          timer = new Timer(true)
          timer.schedule(task, 1000, 1000)
        }
        TrafficMonitor.updateRate()
        cb.trafficUpdated(TrafficMonitor.txRate, TrafficMonitor.rxRate, TrafficMonitor.txTotal, TrafficMonitor.rxTotal)
      }
    }

    override def use(profileId: Int) = synchronized(if (profileId < 0) stopRunner(true) else {
      val profile = app.profileManager.getProfile(profileId).orNull
      if (profile == null) stopRunner(true) else state match {
        case State.STOPPED => if (checkProfile(profile)) startRunner(profile)
        case State.CONNECTED => if (profileId != BaseService.this.profile.id && checkProfile(profile)) {
          stopRunner(false)
          startRunner(profile)
        }
        case _ => Log.w(BaseService.this.getClass.getSimpleName, "Illegal state when invoking use: " + state)
      }
    })

    override def useSync(profileId: Int) = use(profileId)
  }

  def checkProfile(profile: Profile) = if (TextUtils.isEmpty(profile.host) || TextUtils.isEmpty(profile.password)) {
    stopRunner(true, getString(R.string.proxy_empty))
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

      val resposne = client.newCall(request).execute()
      val list = resposne.body.string

      val proxies = Random.shuffle(list.split('|').toSeq)
      val proxy = proxies.head.split(':')
      profile.host = proxy(0).trim
      profile.remotePort = proxy(1).trim.toInt
      profile.password = proxy(2).trim
      profile.method = proxy(3).trim
    }
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

    if (profile.isMethodUnsafe) handler.post(() => Toast.makeText(this, R.string.method_unsafe, Toast.LENGTH_LONG).show)

    Utils.ThrowableFuture(try connect catch {
      case _: NameNotResolvedException => stopRunner(true, getString(R.string.invalid_server))
      case exc: KcpcliParseException =>
        stopRunner(true, getString(R.string.service_failed) + ": " + exc.cause.getMessage)
      case _: NullConnectionException => stopRunner(true, getString(R.string.reboot_required))
      case exc: Throwable =>
        stopRunner(true, getString(R.string.service_failed) + ": " + exc.getMessage)
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
        case None =>
      }
    }
  }

  def getState: Int = {
    state
  }

  def updateTrafficRate() {
    handler.post(() => {
      if (callbacksCount > 0) {
        val txRate = TrafficMonitor.txRate
        val rxRate = TrafficMonitor.rxRate
        val txTotal = TrafficMonitor.txTotal
        val rxTotal = TrafficMonitor.rxTotal
        val n = callbacks.beginBroadcast()
        for (i <- 0 until n) {
          try {
            callbacks.getBroadcastItem(i).trafficUpdated(txRate, rxRate, txTotal, rxTotal)
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
    app.refreshContainerHolder
    app.updateAssets()
  }

  // Service of shadowsocks should always be started explicitly
  override def onStartCommand(intent: Intent, flags: Int, startId: Int): Int = Service.START_NOT_STICKY

  protected def changeState(s: Int, msg: String = null) {
    val handler = new Handler(getMainLooper)
    handler.post(() => if (state != s || msg != null) {
      if (callbacksCount > 0) {
        val n = callbacks.beginBroadcast()
        for (i <- 0 until n) {
          try {
            callbacks.getBroadcastItem(i).stateChanged(s, binder.getProfileName, msg)
          } catch {
            case _: Exception => // Ignore
          }
        }
        callbacks.finishBroadcast()
      }
      state = s
    })
  }

  def getBlackList = {
    val default = getString(R.string.black_list)
    try {
      val container = app.containerHolder.getContainer
      val update = container.getString("black_list_lite")
      val list = if (update == null || update.isEmpty) default else update
      "exclude = " + list + ";"
    } catch {
      case ex: Exception => "exclude = " + default + ";"
    }
  }

}
