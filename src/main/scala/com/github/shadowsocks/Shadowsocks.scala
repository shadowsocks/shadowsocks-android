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

import java.io.{FileOutputStream, IOException, InputStream, OutputStream}
import java.util
import java.util.Locale

import android.app.backup.BackupManager
import android.app.{Activity, ProgressDialog}
import android.content._
import android.content.res.AssetManager
import android.graphics.Typeface
import android.net.VpnService
import android.os._
import android.support.design.widget.{FloatingActionButton, Snackbar}
import android.support.v4.content.ContextCompat
import android.support.v7.app.AppCompatActivity
import android.support.v7.widget.Toolbar
import android.util.Log
import android.view.{View, ViewGroup, ViewParent}
import android.widget._
import com.github.jorgecastilloprz.FABProgressCircle
import com.github.shadowsocks.aidl.IShadowsocksServiceCallback
import com.github.shadowsocks.database._
import com.github.shadowsocks.utils._
import com.google.android.gms.ads.{AdRequest, AdSize, AdView}

import scala.collection.mutable.ArrayBuffer

object Typefaces {
  def get(c: Context, assetPath: String): Typeface = {
    cache synchronized {
      if (!cache.containsKey(assetPath)) {
        try {
          cache.put(assetPath, Typeface.createFromAsset(c.getAssets, assetPath))
        } catch {
          case e: Exception =>
            Log.e(TAG, "Could not get typeface '" + assetPath + "' because " + e.getMessage)
            return null
        }
      }
      return cache.get(assetPath)
    }
  }

  private final val TAG = "Typefaces"
  private final val cache = new util.Hashtable[String, Typeface]
}

object Shadowsocks {

  // Constants
  val TAG = "Shadowsocks"
  val REQUEST_CONNECT = 1
  val EXECUTABLES = Array(Executable.PDNSD, Executable.REDSOCKS, Executable.SS_TUNNEL, Executable.SS_LOCAL,
    Executable.TUN2SOCKS)

}

class Shadowsocks
  extends AppCompatActivity with ServiceBoundContext {

  // Variables
  var serviceStarted = false
  var fab: FloatingActionButton = _
  var fabProgressCircle: FABProgressCircle = _
  var progressDialog: ProgressDialog = _
  var progressTag = -1
  var state = State.STOPPED
  var currentProfile = new Profile
  var vpnEnabled = -1
  var trafficCache: String = _
  var connectionTestResult: String = _

  // Services
  var currentServiceName = classOf[ShadowsocksNatService].getName
  private val callback = new IShadowsocksServiceCallback.Stub {
    def stateChanged(s: Int, m: String) {
      handler.post(() => if (state != s) {
        s match {
          case State.CONNECTING =>
            fab.setBackgroundTintList(greyTint)
            fab.setImageResource(R.drawable.ic_cloud_queue)
            fab.setEnabled(false)
            fabProgressCircle.show()
            preferences.setEnabled(false)
          case State.CONNECTED =>
            fab.setBackgroundTintList(greenTint)
            if (state == State.CONNECTING) {
              fabProgressCircle.beginFinalAnimation()
            } else {
              fabProgressCircle.postDelayed(hideCircle, 1000)
            }
            fab.setEnabled(true)
            changeSwitch(checked = true)
            preferences.setEnabled(false)
          case State.STOPPED =>
            fab.setBackgroundTintList(greyTint)
            fabProgressCircle.postDelayed(hideCircle, 1000)
            fab.setEnabled(true)
            changeSwitch(checked = false)
            if (m != null) {
              val snackbar = Snackbar.make(findViewById(android.R.id.content),
                getString(R.string.vpn_error).formatLocal(Locale.ENGLISH, m), Snackbar.LENGTH_LONG)
              if (m == getString(R.string.nat_no_root)) snackbar.setAction(R.string.switch_to_vpn,
                (_ => preferences.natSwitch.setChecked(false)): View.OnClickListener)
              snackbar.show
            }
            preferences.setEnabled(true)
          case State.STOPPING =>
            fab.setBackgroundTintList(greyTint)
            fab.setImageResource(R.drawable.ic_cloud_queue)
            fab.setEnabled(false)
            if (state == State.CONNECTED) fabProgressCircle.show()  // ignore for stopped
            preferences.setEnabled(false)
        }
        state = s
      })
    }
    def trafficUpdated(txRate: Long, rxRate: Long, txTotal: Long, rxTotal: Long) {
      trafficCache = getString(R.string.stat_summary).formatLocal(Locale.ENGLISH,
        TrafficMonitor.formatTraffic(txRate), TrafficMonitor.formatTraffic(rxRate),
        TrafficMonitor.formatTraffic(txTotal), TrafficMonitor.formatTraffic(rxTotal))
      handler.post(updateTraffic)
    }
  }

  def updateTraffic(): Unit = if (trafficCache == null) callback.trafficUpdated(0, 0, 0, 0) else {
    if (connectionTestResult == null) connectionTestResult = getString(R.string.connection_test_pending)
    if (preferences.natSwitch.isChecked) {
      preferences.stat.setSummary(trafficCache)
    } else {
      preferences.stat.setSummary(connectionTestResult + "\n" + trafficCache)
    }
  }

  def attachService: Unit = attachService(callback)

  override def onServiceConnected() {
    // Update the UI
    if (fab != null) fab.setEnabled(true)
    updateState()

    if (!ShadowsocksApplication.settings.getBoolean(ShadowsocksApplication.getVersionName, false)) {
      ShadowsocksApplication.settings.edit.putBoolean(ShadowsocksApplication.getVersionName, true).apply()
      try {
        // Workaround that convert port(String) to port(Int)
        val oldLocalPort = ShadowsocksApplication.settings.getString(Key.localPort, "")
        val oldRemotePort = ShadowsocksApplication.settings.getString(Key.remotePort, "")

        if (oldLocalPort != "") {
          ShadowsocksApplication.settings.edit.putInt(Key.localPort, oldLocalPort.toInt).apply()
        }
        if (oldRemotePort != "") {
          ShadowsocksApplication.settings.edit.putInt(Key.remotePort, oldRemotePort.toInt).apply()
        }
      } catch {
        case ex: Exception => // Ignore
      }
      val oldProxiedApps = ShadowsocksApplication.settings.getString(Key.proxied, "")
      if (oldProxiedApps.contains('|')) ShadowsocksApplication.settings.edit
        .putString(Key.proxied, DBHelper.updateProxiedApps(this, oldProxiedApps)).apply()

      recovery()
    }
  }

  override def onServiceDisconnected() {
    if (fab != null) fab.setEnabled(false)
  }

  private lazy val preferences =
    getFragmentManager.findFragmentById(android.R.id.content).asInstanceOf[ShadowsocksSettings]
  private var adView: AdView = _
  private lazy val greyTint = ContextCompat.getColorStateList(this, R.color.material_blue_grey_700)
  private lazy val greenTint = ContextCompat.getColorStateList(this, R.color.material_green_700)

  var handler = new Handler()

  private def changeSwitch(checked: Boolean) {
    serviceStarted = checked
    fab.setImageResource(if (checked) R.drawable.ic_cloud else R.drawable.ic_cloud_off)
    if (fab.isEnabled) {
      fab.setEnabled(false)
      handler.postDelayed(() => fab.setEnabled(true), 1000)
    }
  }

  private def showProgress(msg: Int): Handler = {
    clearDialog()
    progressDialog = ProgressDialog.show(this, "", getString(msg), true, false)
    progressTag = msg
    new Handler {
      override def handleMessage(msg: Message) {
        clearDialog()
      }
    }
  }

  private def copyAssets(path: String) {
    val assetManager: AssetManager = getAssets
    var files: Array[String] = null
    try {
      files = assetManager.list(path)
    } catch {
      case e: IOException =>
        Log.e(Shadowsocks.TAG, e.getMessage)
    }
    if (files != null) {
      for (file <- files) {
        var in: InputStream = null
        var out: OutputStream = null
        try {
          if (path.length > 0) {
            in = assetManager.open(path + "/" + file)
          } else {
            in = assetManager.open(file)
          }
          out = new FileOutputStream(Path.BASE + file)
          copyFile(in, out)
          in.close()
          in = null
          out.flush()
          out.close()
          out = null
        } catch {
          case ex: Exception =>
            Log.e(Shadowsocks.TAG, ex.getMessage)
        }
      }
    }
  }

  private def copyFile(in: InputStream, out: OutputStream) {
    val buffer: Array[Byte] = new Array[Byte](1024)
    var read: Int = 0
    while ( {
      read = in.read(buffer)
      read
    } != -1) {
      out.write(buffer, 0, read)
    }
  }

  private def crashRecovery() {
    val cmd = new ArrayBuffer[String]()

    for (task <- Array("ss-local", "ss-tunnel", "pdnsd", "redsocks", "tun2socks")) {
      cmd.append("chmod 666 %s%s-nat.pid".formatLocal(Locale.ENGLISH, Path.BASE, task))
      cmd.append("chmod 666 %s%s-vpn.pid".formatLocal(Locale.ENGLISH, Path.BASE, task))
    }

    if (ShadowsocksApplication.isVpnEnabled) {
      Console.runCommand(cmd.toArray)
    } else {
      Console.runRootCommand(cmd.toArray)
    }

    cmd.clear()

    for (task <- Array("ss-local", "ss-tunnel", "pdnsd", "redsocks", "tun2socks")) {
      try {
        val pid_nat = scala.io.Source.fromFile(Path.BASE + task + "-nat.pid").mkString.trim.toInt
        val pid_vpn = scala.io.Source.fromFile(Path.BASE + task + "-vpn.pid").mkString.trim.toInt
        cmd.append("kill -9 %d".formatLocal(Locale.ENGLISH, pid_nat))
        cmd.append("kill -9 %d".formatLocal(Locale.ENGLISH, pid_vpn))
        Process.killProcess(pid_nat)
        Process.killProcess(pid_vpn)
      } catch {
        case e: Throwable => Log.e(Shadowsocks.TAG, "unable to kill " + task)
      }
      cmd.append("rm -f %s%s-nat.pid".formatLocal(Locale.ENGLISH, Path.BASE, task))
      cmd.append("rm -f %s%s-nat.conf".formatLocal(Locale.ENGLISH, Path.BASE, task))
      cmd.append("rm -f %s%s-vpn.pid".formatLocal(Locale.ENGLISH, Path.BASE, task))
      cmd.append("rm -f %s%s-vpn.conf".formatLocal(Locale.ENGLISH, Path.BASE, task))
    }
    if (ShadowsocksApplication.isVpnEnabled) Console.runCommand(cmd.toArray) else {
      Console.runRootCommand(cmd.toArray)
      Console.runRootCommand(Utils.getIptables + " -t nat -F OUTPUT")
    }
  }

  def cancelStart() {
    clearDialog()
    changeSwitch(checked = false)
  }

  def isReady: Boolean = checkText(Key.proxy) && checkText(Key.sitekey) && bgService != null

  def prepareStartService() {
    ThrowableFuture {
      if (ShadowsocksApplication.isVpnEnabled) {
        val intent = VpnService.prepare(this)
        if (intent != null) {
          startActivityForResult(intent, Shadowsocks.REQUEST_CONNECT)
        } else {
          handler.post(() => onActivityResult(Shadowsocks.REQUEST_CONNECT, Activity.RESULT_OK, null))
        }
      } else {
        serviceStart()
      }
    }
  }

  def getLayoutView(view: ViewParent): LinearLayout = {
    view match {
      case layout: LinearLayout => layout
      case _ => if (view != null) getLayoutView(view.getParent) else null
    }
  }

  override def onCreate(savedInstanceState: Bundle) {

    super.onCreate(savedInstanceState)
    setContentView(R.layout.layout_main)
    // Initialize Toolbar
    val toolbar = findViewById(R.id.toolbar).asInstanceOf[Toolbar]
    toolbar.setTitle(getString(R.string.screen_name))
    toolbar.setTitleTextAppearance(toolbar.getContext, R.style.Toolbar_Logo)
    val field = classOf[Toolbar].getDeclaredField("mTitleTextView")
    field.setAccessible(true)
    val title: TextView = field.get(toolbar).asInstanceOf[TextView]
    val tf: Typeface = Typefaces.get(this, "fonts/Iceland.ttf")
    if (tf != null) title.setTypeface(tf)

    fab = findViewById(R.id.fab).asInstanceOf[FloatingActionButton]
    fabProgressCircle = findViewById(R.id.fabProgressCircle).asInstanceOf[FABProgressCircle]
    fab.setOnClickListener((v: View) => {
      serviceStarted = !serviceStarted
      serviceStarted match {
        case true =>
          if (isReady)
            prepareStartService()
          else
            changeSwitch(checked = false)
        case false =>
          serviceStop()
      }
    })
    fab.setOnLongClickListener((v: View) => {
      Utils.positionToast(Toast.makeText(this, if (serviceStarted) R.string.stop else R.string.connect,
        Toast.LENGTH_SHORT), fab, getWindow, 0, Utils.dpToPx(this, 8)).show
      true
    })

    // Bind to the service
    handler.post(() => attachService)
  }

  def reloadProfile() {
    currentProfile = ShadowsocksApplication.currentProfile match {
      case Some(profile) => profile // updated
      case None =>                  // removed
        ShadowsocksApplication.profileManager.getFirstProfile match {
          case Some(first) => ShadowsocksApplication.switchProfile(first.id)
          case None => ShadowsocksApplication.profileManager.createDefault()
        }
    }

    updatePreferenceScreen()

    serviceStop()
  }

  protected override def onPause() {
    super.onPause()
    ShadowsocksApplication.profileManager.save
  }

  private def hideCircle() {
    try {
      fabProgressCircle.hide()
    } catch {
      case _: java.lang.NullPointerException => // Ignore
    }
  }

  private def updateState() {
    if (bgService != null) {
      bgService.getState match {
        case State.CONNECTING =>
          fab.setBackgroundTintList(greyTint)
          serviceStarted = false
          fab.setImageResource(R.drawable.ic_cloud_queue)
          preferences.setEnabled(false)
          fabProgressCircle.show()
        case State.CONNECTED =>
          fab.setBackgroundTintList(greenTint)
          serviceStarted = true
          fab.setImageResource(R.drawable.ic_cloud)
          preferences.setEnabled(false)
          fabProgressCircle.postDelayed(hideCircle, 100)
        case State.STOPPING =>
          fab.setBackgroundTintList(greyTint)
          serviceStarted = false
          fab.setImageResource(R.drawable.ic_cloud_queue)
          preferences.setEnabled(false)
          fabProgressCircle.show()
        case _ =>
          fab.setBackgroundTintList(greyTint)
          serviceStarted = false
          fab.setImageResource(R.drawable.ic_cloud_off)
          preferences.setEnabled(true)
          fabProgressCircle.postDelayed(hideCircle, 100)
      }
      state = bgService.getState
    }
  }

  protected override def onResume() {
    super.onResume()
    ConfigUtils.refresh(this)

    // Check if current profile changed
    if (ShadowsocksApplication.profileId != currentProfile.id) reloadProfile()

    updateTraffic()
    updateState()
  }

  private def updatePreferenceScreen() {
    val profile = currentProfile
    if (profile.host == "198.199.101.152") if (adView == null) {
      adView = new AdView(this)
      adView.setAdUnitId("ca-app-pub-9097031975646651/7760346322")
      adView.setAdSize(AdSize.SMART_BANNER)
      preferences.getView.asInstanceOf[ViewGroup].addView(adView, 0)
      adView.loadAd(new AdRequest.Builder().build())
    } else adView.setVisibility(View.VISIBLE) else if (adView != null) adView.setVisibility(View.GONE)

    preferences.update(profile)
  }

  override def onStart() {
    super.onStart()
    registerCallback
  }
  override def onStop() {
    super.onStop()
    unregisterCallback
    clearDialog()
  }

  private var _isDestroyed: Boolean = _
  override def isDestroyed = if (Build.VERSION.SDK_INT >= 17) super.isDestroyed else _isDestroyed
  override def onDestroy() {
    super.onDestroy()
    _isDestroyed = true
    deattachService()
    new BackupManager(this).dataChanged()
    handler.removeCallbacksAndMessages(null)
  }

  def install() {
    copyAssets(System.getABI)

    val ab = new ArrayBuffer[String]
    for (executable <- Shadowsocks.EXECUTABLES) {
      ab.append("chmod 755 " + Path.BASE + executable)
    }
    Console.runCommand(ab.toArray)
  }

  def reset() {
    crashRecovery()

    install()
  }

  def recovery() {
    serviceStop()
    val h = showProgress(R.string.recovering)
    ThrowableFuture {
      reset()
      h.sendEmptyMessage(0)
    }
  }

  def flushDnsCache() {
    val h = showProgress(R.string.flushing)
    ThrowableFuture {
      if (!Utils.toggleAirplaneMode(getBaseContext)) h.post(() => Snackbar.make(findViewById(android.R.id.content),
        R.string.flush_dnscache_no_root, Snackbar.LENGTH_LONG).show)
      h.sendEmptyMessage(0)
    }
  }

  override def onActivityResult(requestCode: Int, resultCode: Int, data: Intent) = resultCode match {
    case Activity.RESULT_OK =>
      serviceStart()
    case _ =>
      cancelStart()
      Log.e(Shadowsocks.TAG, "Failed to start VpnService")
  }

  def serviceStop() {
    if (bgService != null) bgService.stop()
  }

  def checkText(key: String): Boolean = {
    val text = ShadowsocksApplication.settings.getString(key, "")
    if (text != null && text.length > 0) return true
    Snackbar.make(findViewById(android.R.id.content), R.string.proxy_empty, Snackbar.LENGTH_LONG).show
    false
  }

  /** Called when connect button is clicked. */
  def serviceStart() {
    bgService.start(ConfigUtils.load(ShadowsocksApplication.settings))

    if (ShadowsocksApplication.isVpnEnabled) {
      changeSwitch(checked = false)
    }
  }

  def clearDialog() {
    if (progressDialog != null && progressDialog.isShowing) {
      if (!isDestroyed) progressDialog.dismiss()
      progressDialog = null
      progressTag = -1
    }
  }
}
