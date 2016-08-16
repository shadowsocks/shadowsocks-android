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
import java.lang.System.currentTimeMillis
import java.net.{HttpURLConnection, URL}
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
import android.view.{View, ViewGroup}
import android.widget._
import com.github.jorgecastilloprz.FABProgressCircle
import com.github.shadowsocks.aidl.IShadowsocksServiceCallback
import com.github.shadowsocks.database._
import com.github.shadowsocks.utils.CloseUtils._
import com.github.shadowsocks.utils._
import com.github.shadowsocks.ShadowsocksApplication.app
import com.google.android.gms.ads.{AdRequest, AdSize, AdView}
import eu.chainfire.libsuperuser.Shell

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
            app.track(e)
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
  private final val TAG = "Shadowsocks"
  private final val REQUEST_CONNECT = 1
  private val EXECUTABLES = Array(Executable.PDNSD, Executable.REDSOCKS, Executable.SS_TUNNEL, Executable.SS_LOCAL,
    Executable.TUN2SOCKS, Executable.KCPTUN)
}

class Shadowsocks extends AppCompatActivity with ServiceBoundContext {
  import Shadowsocks._

  // Variables
  var serviceStarted: Boolean = _
  var fab: FloatingActionButton = _
  var fabProgressCircle: FABProgressCircle = _
  var progressDialog: ProgressDialog = _
  var state = State.STOPPED
  var currentProfile = new Profile

  // Services
  private val callback = new IShadowsocksServiceCallback.Stub {
    def stateChanged(s: Int, m: String) {
      handler.post(() => {
        s match {
          case State.CONNECTING =>
            fab.setBackgroundTintList(greyTint)
            fab.setImageResource(R.drawable.ic_start_busy)
            fab.setEnabled(false)
            fabProgressCircle.show()
            preferences.setEnabled(false)
            stat.setVisibility(View.GONE)
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
            stat.setVisibility(View.VISIBLE)
            if (app.isNatEnabled) connectionTestText.setVisibility(View.GONE) else {
              connectionTestText.setVisibility(View.VISIBLE)
              connectionTestText.setText(getString(R.string.connection_test_pending))
            }
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
              Log.e(TAG, "Error to start VPN service: " + m)
            }
            preferences.setEnabled(true)
            stat.setVisibility(View.GONE)
          case State.STOPPING =>
            fab.setBackgroundTintList(greyTint)
            fab.setImageResource(R.drawable.ic_start_busy)
            fab.setEnabled(false)
            if (state == State.CONNECTED) fabProgressCircle.show()  // ignore for stopped
            preferences.setEnabled(false)
            stat.setVisibility(View.GONE)
        }
        state = s
      })
    }
    def trafficUpdated(txRate: Long, rxRate: Long, txTotal: Long, rxTotal: Long) {
      handler.post(() => updateTraffic(txRate, rxRate, txTotal, rxTotal))
    }
  }

  def updateTraffic(txRate: Long, rxRate: Long, txTotal: Long, rxTotal: Long) {
    txText.setText(TrafficMonitor.formatTraffic(txTotal))
    rxText.setText(TrafficMonitor.formatTraffic(rxTotal))
    txRateText.setText(TrafficMonitor.formatTraffic(txRate) + "/s")
    rxRateText.setText(TrafficMonitor.formatTraffic(rxRate) + "/s")
  }

  def attachService: Unit = attachService(callback)

  override def onServiceConnected() {
    // Update the UI
    if (fab != null) fab.setEnabled(true)
    updateState()
    if (Build.VERSION.SDK_INT >= 21 && app.isNatEnabled) {
      val snackbar = Snackbar.make(findViewById(android.R.id.content), R.string.nat_deprecated, Snackbar.LENGTH_LONG)
      snackbar.setAction(R.string.switch_to_vpn, (_ => preferences.natSwitch.setChecked(false)): View.OnClickListener)
      snackbar.show
    }

    if (!app.settings.getBoolean(BuildConfig.VERSION_NAME, false)) {
      app.editor.putBoolean(BuildConfig.VERSION_NAME, true).apply()

      recovery()

      updateCurrentProfile()
    }
  }

  override def onServiceDisconnected() {
    if (fab != null) fab.setEnabled(false)
  }

  override def binderDied {
    detachService
    crashRecovery
    attachService
  }

  private var testCount: Int = _
  private var stat: View = _
  private var connectionTestText: TextView = _
  private var txText: TextView = _
  private var rxText: TextView = _
  private var txRateText: TextView = _
  private var rxRateText: TextView = _
  private lazy val greyTint = ContextCompat.getColorStateList(this, R.color.material_blue_grey_700)
  private lazy val greenTint = ContextCompat.getColorStateList(this, R.color.material_green_700)
  private var adView: AdView = _
  private lazy val preferences =
    getFragmentManager.findFragmentById(android.R.id.content).asInstanceOf[ShadowsocksSettings]

  val handler = new Handler()

  private def changeSwitch(checked: Boolean) {
    serviceStarted = checked
    fab.setImageResource(if (checked) R.drawable.ic_start_connected else R.drawable.ic_start_idle)
    if (fab.isEnabled) {
      fab.setEnabled(false)
      handler.postDelayed(() => fab.setEnabled(true), 1000)
    }
  }

  private def showProgress(msg: Int): Handler = {
    clearDialog()
    progressDialog = ProgressDialog.show(this, "", getString(msg), true, false)
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
        Log.e(TAG, e.getMessage)
        app.track(e)
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
          out = new FileOutputStream(getApplicationInfo.dataDir + "/" + file)
          copyFile(in, out)
          in.close()
          in = null
          out.flush()
          out.close()
          out = null
        } catch {
          case ex: Exception =>
            Log.e(TAG, ex.getMessage)
            app.track(ex)
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

  def crashRecovery() {
    val cmd = new ArrayBuffer[String]()

    for (task <- Array("ss-local", "ss-tunnel", "pdnsd", "redsocks", "tun2socks", "kcptun")) {
      cmd.append("killall %s".formatLocal(Locale.ENGLISH, task))
      cmd.append("rm -f %1$s/%2$s-nat.conf %1$s/%2$s-vpn.conf"
        .formatLocal(Locale.ENGLISH, getApplicationInfo.dataDir, task))
    }
    if (app.isNatEnabled) {
      cmd.append("iptables -t nat -F OUTPUT")
      cmd.append("echo done")
      val result = Shell.SU.run(cmd.toArray)
      if (result != null && !result.isEmpty) return // fallback to SH
    }
    Shell.SH.run(cmd.toArray)
  }

  def cancelStart() {
    clearDialog()
    changeSwitch(checked = false)
  }

  def prepareStartService() {
    Utils.ThrowableFuture {
      if (app.isNatEnabled) serviceLoad() else {
        val intent = VpnService.prepare(this)
        if (intent != null) {
          startActivityForResult(intent, REQUEST_CONNECT)
        } else {
          handler.post(() => onActivityResult(REQUEST_CONNECT, Activity.RESULT_OK, null))
        }
      }
    }
  }

  override def onCreate(savedInstanceState: Bundle) {

    super.onCreate(savedInstanceState)
    setContentView(R.layout.layout_main)
    // Initialize Toolbar
    val toolbar = findViewById(R.id.toolbar).asInstanceOf[Toolbar]
    toolbar.setTitle("shadowsocks") // non-translatable logo
    toolbar.setTitleTextAppearance(toolbar.getContext, R.style.Toolbar_Logo)
    val field = classOf[Toolbar].getDeclaredField("mTitleTextView")
    field.setAccessible(true)
    val title = field.get(toolbar).asInstanceOf[TextView]
    title.setFocusable(true)
    title.setGravity(0x10)
    title.getLayoutParams.height = ViewGroup.LayoutParams.MATCH_PARENT
    title.setOnClickListener(_ => startActivity(new Intent(this, classOf[ProfileManagerActivity])))
    val typedArray = obtainStyledAttributes(Array(R.attr.selectableItemBackgroundBorderless))
    title.setBackgroundResource(typedArray.getResourceId(0, 0))
    typedArray.recycle
    val tf = Typefaces.get(this, "fonts/Iceland.ttf")
    if (tf != null) title.setTypeface(tf)
    title.setCompoundDrawablesWithIntrinsicBounds(0, 0, R.drawable.ic_arrow_drop_down, 0)

    stat = findViewById(R.id.stat)
    connectionTestText = findViewById(R.id.connection_test).asInstanceOf[TextView]
    txText = findViewById(R.id.tx).asInstanceOf[TextView]
    txRateText = findViewById(R.id.txRate).asInstanceOf[TextView]
    rxText = findViewById(R.id.rx).asInstanceOf[TextView]
    rxRateText = findViewById(R.id.rxRate).asInstanceOf[TextView]
    stat.setOnClickListener(_ => {
      val id = synchronized {
        testCount += 1
        handler.post(() => connectionTestText.setText(R.string.connection_test_testing))
        testCount
      }
      Utils.ThrowableFuture {
        // Based on: https://android.googlesource.com/platform/frameworks/base/+/master/services/core/java/com/android/server/connectivity/NetworkMonitor.java#640
        autoDisconnect(new URL("https", "www.google.com", "/generate_204").openConnection()
          .asInstanceOf[HttpURLConnection]) { conn =>
          conn.setConnectTimeout(5 * 1000)
          conn.setReadTimeout(5 * 1000)
          conn.setInstanceFollowRedirects(false)
          conn.setUseCaches(false)
          if (testCount == id) {
            var result: String = null
            var success = true
            try {
              val start = currentTimeMillis
              conn.getInputStream
              val elapsed = currentTimeMillis - start
              val code = conn.getResponseCode
              if (code == 204 || code == 200 && conn.getContentLength == 0)
                result = getString(R.string.connection_test_available, elapsed: java.lang.Long)
              else throw new Exception(getString(R.string.connection_test_error_status_code, code: Integer))
            } catch {
              case e: Exception =>
                success = false
                result = getString(R.string.connection_test_error, e.getMessage)
            }
            synchronized(if (testCount == id && app.isVpnEnabled) handler.post(() =>
              if (success) connectionTestText.setText(result) else {
                connectionTestText.setText(R.string.connection_test_fail)
                Snackbar.make(findViewById(android.R.id.content), result, Snackbar.LENGTH_LONG).show
              }))
          }
        }
      }
    })

    fab = findViewById(R.id.fab).asInstanceOf[FloatingActionButton]
    fabProgressCircle = findViewById(R.id.fabProgressCircle).asInstanceOf[FABProgressCircle]
    fab.setOnClickListener(_ => if (serviceStarted) serviceStop()
      else if (bgService != null) prepareStartService()
      else changeSwitch(checked = false))
    fab.setOnLongClickListener((v: View) => {
      Utils.positionToast(Toast.makeText(this, if (serviceStarted) R.string.stop else R.string.connect,
        Toast.LENGTH_SHORT), fab, getWindow, 0, Utils.dpToPx(this, 8)).show
      true
    })
    updateTraffic(0, 0, 0, 0)

    handler.post(() => attachService)
  }

  private def hideCircle() {
    try {
      fabProgressCircle.hide()
    } catch {
      case _: java.lang.NullPointerException => // Ignore
    }
  }

  private def updateState(resetConnectionTest: Boolean = true) {
    if (bgService != null) {
      bgService.getState match {
        case State.CONNECTING =>
          fab.setBackgroundTintList(greyTint)
          serviceStarted = false
          fab.setImageResource(R.drawable.ic_start_busy)
          preferences.setEnabled(false)
          fabProgressCircle.show()
          stat.setVisibility(View.GONE)
        case State.CONNECTED =>
          fab.setBackgroundTintList(greenTint)
          serviceStarted = true
          fab.setImageResource(R.drawable.ic_start_connected)
          preferences.setEnabled(false)
          fabProgressCircle.postDelayed(hideCircle, 100)
          stat.setVisibility(View.VISIBLE)
          if (resetConnectionTest) if (app.isNatEnabled) connectionTestText.setVisibility(View.GONE) else {
            connectionTestText.setVisibility(View.VISIBLE)
            connectionTestText.setText(getString(R.string.connection_test_pending))
          }
        case State.STOPPING =>
          fab.setBackgroundTintList(greyTint)
          serviceStarted = false
          fab.setImageResource(R.drawable.ic_start_busy)
          preferences.setEnabled(false)
          fabProgressCircle.show()
          stat.setVisibility(View.GONE)
        case _ =>
          fab.setBackgroundTintList(greyTint)
          serviceStarted = false
          fab.setImageResource(R.drawable.ic_start_idle)
          preferences.setEnabled(true)
          fabProgressCircle.postDelayed(hideCircle, 100)
          stat.setVisibility(View.GONE)
      }
      state = bgService.getState
    }
  }

  private def updateCurrentProfile() = {
    // Check if current profile changed
    if (preferences.profile == null || app.profileId != preferences.profile.id) {
      updatePreferenceScreen(app.currentProfile match {
        case Some(profile) => profile // updated
        case None =>                  // removed
          app.switchProfile((app.profileManager.getFirstProfile match {
            case Some(first) => first
            case None => app.profileManager.createDefault()
          }).id)
      })

      if (serviceStarted) serviceLoad()

      true
    } else {
      preferences.refreshProfile()
      false
    }
  }

  protected override def onResume() {
    super.onResume()

    app.refreshContainerHolder

    updateState(updateCurrentProfile())
  }

  private def updatePreferenceScreen(profile: Profile) {
    if (profile.host == "198.199.101.152") if (adView == null) {
      adView = new AdView(this)
      adView.setAdUnitId("ca-app-pub-9097031975646651/7760346322")
      adView.setAdSize(AdSize.SMART_BANNER)
      preferences.getView.asInstanceOf[ViewGroup].addView(adView, 1)
      adView.loadAd(new AdRequest.Builder().build())
    } else adView.setVisibility(View.VISIBLE) else if (adView != null) adView.setVisibility(View.GONE)

    preferences.setProfile(profile)
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

  override def onDestroy() {
    super.onDestroy()
    detachService()
    new BackupManager(this).dataChanged()
    handler.removeCallbacksAndMessages(null)
  }

  def reset() {
    crashRecovery()

    copyAssets(System.getABI)

    val ab = new ArrayBuffer[String]
    for (executable <- EXECUTABLES) {
      ab.append("chmod 755 " + getApplicationInfo.dataDir + "/" + executable)
    }
    Shell.SH.run(ab.toArray)
  }

  def recovery() {
    if (serviceStarted) serviceStop()
    val h = showProgress(R.string.recovering)
    Utils.ThrowableFuture {
      reset()
      h.sendEmptyMessage(0)
    }
  }

  def flushDnsCache() {
    val h = showProgress(R.string.flushing)
    Utils.ThrowableFuture {
      if (!Utils.toggleAirplaneMode(getBaseContext)) h.post(() => Snackbar.make(findViewById(android.R.id.content),
        R.string.flush_dnscache_no_root, Snackbar.LENGTH_LONG).show)
      h.sendEmptyMessage(0)
    }
  }

  override def onActivityResult(requestCode: Int, resultCode: Int, data: Intent) = resultCode match {
    case Activity.RESULT_OK =>
      serviceLoad()
    case _ =>
      cancelStart()
      Log.e(TAG, "Failed to start VpnService")
  }

  def serviceStop() {
    if (bgService != null) bgService.use(-1)
  }

  /** Called when connect button is clicked. */
  def serviceLoad() {
    bgService.use(app.profileId)

    if (app.isVpnEnabled) {
      changeSwitch(checked = false)
    }
  }

  def clearDialog() {
    if (progressDialog != null && progressDialog.isShowing) {
      if (!isDestroyed) progressDialog.dismiss()
      progressDialog = null
    }
  }
}
