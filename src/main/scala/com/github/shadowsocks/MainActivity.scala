/*******************************************************************************/
/*                                                                             */
/*  Copyright (C) 2016 by Max Lv <max.c.lv@gmail.com>                          */
/*  Copyright (C) 2016 by Mygod Studio <contact-shadowsocks-android@mygod.be>  */
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

import java.lang.System.currentTimeMillis
import java.net.{HttpURLConnection, URL}
import java.util.Locale

import android.app.{Activity, ProgressDialog}
import android.app.backup.BackupManager
import android.content.SharedPreferences.OnSharedPreferenceChangeListener
import android.content._
import android.net.{Uri, VpnService}
import android.nfc.{NdefMessage, NfcAdapter}
import android.os.{Build, Bundle, Handler, Message}
import android.support.design.widget.{FloatingActionButton, Snackbar}
import android.support.v4.content.ContextCompat
import android.support.v7.app.AlertDialog
import android.support.v7.widget.RecyclerView.ViewHolder
import android.text.TextUtils
import android.util.Log
import android.view.View
import android.webkit.{WebView, WebViewClient}
import android.widget.{TextView, Toast}
import com.github.jorgecastilloprz.FABProgressCircle
import com.github.shadowsocks.ShadowsocksApplication.app
import com.github.shadowsocks.aidl.IShadowsocksServiceCallback
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.utils.CloseUtils.autoDisconnect
import com.github.shadowsocks.utils._
import com.google.android.gms.ads.AdView
import com.mikepenz.materialdrawer.model.interfaces.IDrawerItem
import com.mikepenz.materialdrawer.model.{PrimaryDrawerItem, SecondaryDrawerItem}
import com.mikepenz.materialdrawer.{Drawer, DrawerBuilder}

object MainActivity {
  private final val TAG = "ShadowsocksMainActivity"
  private final val REQUEST_CONNECT = 1

  private final val DRAWER_PROFILES = 0L
  // sticky drawer items have negative ids
  private final val DRAWER_RECOVERY = -1L
  private final val DRAWER_GLOBAL_SETTINGS = -2L
  private final val DRAWER_ABOUT = -3L
}

class MainActivity extends Activity with ServiceBoundContext with Drawer.OnDrawerItemClickListener
  with OnSharedPreferenceChangeListener {
  import MainActivity._

  // Variables
  var serviceStarted: Boolean = _
  var fab: FloatingActionButton = _
  var fabProgressCircle: FABProgressCircle = _
  var state = State.STOPPED
  var currentProfile = new Profile
  var drawer: Drawer = _

  private var progressDialog: ProgressDialog = _
  private var currentFragment: ToolbarFragment = _
  private lazy val profilesFragment = new ProfilesFragment()
  private lazy val globalSettingsFragment = new GlobalSettingsFragment()

  // Services
  private val callback = new IShadowsocksServiceCallback.Stub {
    def stateChanged(s: Int, profileName: String, m: String) {
      handler.post(() => {
        s match {
          case State.CONNECTING =>
            fab.setBackgroundTintList(greyTint)
            fab.setImageResource(R.drawable.ic_start_busy)
            fab.setEnabled(false)
            fabProgressCircle.show()
            //stat.setVisibility(View.GONE)
          case State.CONNECTED =>
            fab.setBackgroundTintList(greenTint)
            if (state == State.CONNECTING) {
              fabProgressCircle.beginFinalAnimation()
            } else {
              fabProgressCircle.postDelayed(hideCircle, 1000)
            }
            fab.setEnabled(true)
            changeSwitch(checked = true)
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
              if (m == getString(R.string.nat_no_root)) addDisableNatToSnackbar(snackbar)
              snackbar.show()
              Log.e(TAG, "Error to start VPN service: " + m)
            }
            //stat.setVisibility(View.GONE)
          case State.STOPPING =>
            fab.setBackgroundTintList(greyTint)
            fab.setImageResource(R.drawable.ic_start_busy)
            fab.setEnabled(false)
            if (state == State.CONNECTED) fabProgressCircle.show()  // ignore for stopped
            //stat.setVisibility(View.GONE)
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
    val child = getFragmentManager.findFragmentById(R.id.content).asInstanceOf[ToolbarFragment]
    if (child != null) child.onTrafficUpdated(txRate, rxRate, txTotal, rxTotal)
  }

  def attachServiceCallback(): Unit = attachService(callback)

  override def onServiceConnected() {
    // Update the UI
    if (fab != null) fab.setEnabled(true)
    updateState()
    if (Build.VERSION.SDK_INT >= 21 && app.isNatEnabled) {
      val snackbar = Snackbar.make(findViewById(android.R.id.content), R.string.nat_deprecated, Snackbar.LENGTH_LONG)
      addDisableNatToSnackbar(snackbar)
      snackbar.show()
    }
  }

  private def addDisableNatToSnackbar(snackbar: Snackbar) = snackbar.setAction(R.string.switch_to_vpn, (_ =>
    if (state == State.STOPPED) app.editor.putBoolean(Key.isNAT, false)): View.OnClickListener)

  override def onServiceDisconnected() {
    if (fab != null) fab.setEnabled(false)
  }

  override def binderDied() {
    detachService()
    app.crashRecovery()
    attachServiceCallback()
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

  val handler = new Handler()
  private val connectedListener: BroadcastReceiver = (_, _) =>
    if (app.isNatEnabled) connectionTestText.setVisibility(View.GONE) else {
      connectionTestText.setVisibility(View.VISIBLE)
      connectionTestText.setText(getString(R.string.connection_test_pending))
    }

  private def changeSwitch(checked: Boolean) {
    serviceStarted = checked
    fab.setImageResource(if (checked) R.drawable.ic_start_connected else R.drawable.ic_start_idle)
    if (fab.isEnabled) {
      fab.setEnabled(false)
      handler.postDelayed(() => fab.setEnabled(true), 1000)
    }
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
    drawer = new DrawerBuilder()
      .withActivity(this)
      .withHeader(R.layout.layout_header)
      .addDrawerItems(
        new PrimaryDrawerItem()
          .withIdentifier(DRAWER_PROFILES)
          .withName(R.string.profiles)
          .withIcon(ContextCompat.getDrawable(this, R.drawable.ic_action_description))
          .withIconTintingEnabled(true)
      )
      .addStickyDrawerItems(
        new SecondaryDrawerItem()
          .withIdentifier(DRAWER_RECOVERY)
          .withName(R.string.recovery)
          .withIcon(ContextCompat.getDrawable(this, R.drawable.ic_navigation_refresh))
          .withIconTintingEnabled(true)
          .withSelectable(false),
        new SecondaryDrawerItem()
          .withIdentifier(DRAWER_GLOBAL_SETTINGS)
          .withName(R.string.settings)
          .withIcon(ContextCompat.getDrawable(this, R.drawable.ic_action_settings))
          .withIconTintingEnabled(true),
        new SecondaryDrawerItem()
          .withIdentifier(DRAWER_ABOUT)
          .withName(R.string.about)
          .withIcon(ContextCompat.getDrawable(this, R.drawable.ic_action_copyright))
          .withIconTintingEnabled(true)
          .withSelectable(false)
      )
      .withOnDrawerItemClickListener(this)
      .withActionBarDrawerToggle(true)
      .build()

    val header = drawer.getHeader
    val title = header.findViewById(R.id.drawer_title).asInstanceOf[TextView]
    val tf = Typefaces.get(this, "fonts/Iceland.ttf")
    if (tf != null) title.setTypeface(tf)

    if (savedInstanceState == null) displayFragment(profilesFragment)
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
                Snackbar.make(findViewById(android.R.id.content), result, Snackbar.LENGTH_LONG).show()
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
        Toast.LENGTH_SHORT), fab, getWindow, 0, Utils.dpToPx(this, 8)).show()
      true
    })
    updateTraffic(0, 0, 0, 0)

    handler.post(attachServiceCallback)
    app.settings.registerOnSharedPreferenceChangeListener(this)
    registerReceiver(connectedListener, new IntentFilter(Action.CONNECTED))

    val intent = getIntent
    if (intent != null) handleShareIntent(intent)
  }

  override def onNewIntent(intent: Intent) {
    super.onNewIntent(intent)
    handleShareIntent(intent)
  }

  def handleShareIntent(intent: Intent) {
    val sharedStr = intent.getAction match {
      case Intent.ACTION_VIEW => intent.getData.toString
      case NfcAdapter.ACTION_NDEF_DISCOVERED =>
        val rawMsgs = intent.getParcelableArrayExtra(NfcAdapter.EXTRA_NDEF_MESSAGES)
        if (rawMsgs != null && rawMsgs.nonEmpty)
          new String(rawMsgs(0).asInstanceOf[NdefMessage].getRecords()(0).getPayload)
        else null
      case _ => null
    }
    if (TextUtils.isEmpty(sharedStr)) return
    val profiles = Parser.findAll(sharedStr).toList
    if (profiles.isEmpty) {
      // TODO: show error msg
      return
    }
    val dialog = new AlertDialog.Builder(this)
      .setTitle(R.string.add_profile_dialog)
      .setPositiveButton("Yes", ((_, _) =>  // TODO
        profiles.foreach(app.profileManager.createProfile)): DialogInterface.OnClickListener)
      .setNegativeButton("No", null)
      .setMessage(profiles.mkString("\n"))
      .create()
    dialog.show()
  }

  def onSharedPreferenceChanged(pref: SharedPreferences, key: String): Unit = key match {
    case Key.isNAT => handler.post(() => {
      detachService()
      attachServiceCallback()
    })
    case _ =>
  }

  private def displayFragment(fragment: ToolbarFragment) {
    currentFragment = fragment
    getFragmentManager.beginTransaction().replace(R.id.content, fragment).commitAllowingStateLoss()
    drawer.closeDrawer()
  }

  override def onItemClick(view: View, position: Int, drawerItem: IDrawerItem[_, _ <: ViewHolder]): Boolean = {
    drawerItem.getIdentifier match {
      case DRAWER_PROFILES => displayFragment(profilesFragment)
      case DRAWER_RECOVERY =>
        app.track("GlobalConfigFragment", "reset")
        serviceStop()
        val handler = showProgress(R.string.recovering)
        Utils.ThrowableFuture {
          app.copyAssets()
          handler.sendEmptyMessage(0)
        }
      case DRAWER_GLOBAL_SETTINGS => displayFragment(globalSettingsFragment)
      case DRAWER_ABOUT =>
        app.track(TAG, "about")
        val web = new WebView(this)
        web.loadUrl("file:///android_asset/pages/about.html")
        web.setWebViewClient(new WebViewClient() {
          override def shouldOverrideUrlLoading(view: WebView, url: String): Boolean = {
            try startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(url))) catch {
              case _: android.content.ActivityNotFoundException => // Ignore
            }
            true
          }
        })
        new AlertDialog.Builder(this)
          .setTitle(getString(R.string.about_title).formatLocal(Locale.ENGLISH, BuildConfig.VERSION_NAME))
          .setNegativeButton(getString(android.R.string.ok), null)
          .setView(web)
          .create()
          .show()
    }
    true  // unexpected cases will throw exception
  }

  private def showProgress(msg: Int): Handler = {
    clearDialog()
    progressDialog = ProgressDialog.show(this, "", getString(msg), true, false)
    new Handler {
      override def handleMessage(msg: Message): Unit = clearDialog()
    }
  }

  private def clearDialog() {
    if (progressDialog != null && progressDialog.isShowing) {
      if (!isDestroyed) progressDialog.dismiss()
      progressDialog = null
    }
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
      Log.d(TAG, "bgService " + bgService.getState)
      bgService.getState match {
        case State.CONNECTING =>
          fab.setBackgroundTintList(greyTint)
          serviceStarted = false
          fab.setImageResource(R.drawable.ic_start_busy)
          fabProgressCircle.show()
          //stat.setVisibility(View.GONE)
        case State.CONNECTED =>
          fab.setBackgroundTintList(greenTint)
          serviceStarted = true
          fab.setImageResource(R.drawable.ic_start_connected)
          fabProgressCircle.postDelayed(hideCircle, 100)
          stat.setVisibility(View.VISIBLE)
        case State.STOPPING =>
          fab.setBackgroundTintList(greyTint)
          serviceStarted = false
          fab.setImageResource(R.drawable.ic_start_busy)
          fabProgressCircle.show()
          //stat.setVisibility(View.GONE)
        case _ =>
          fab.setBackgroundTintList(greyTint)
          serviceStarted = false
          fab.setImageResource(R.drawable.ic_start_idle)
          fabProgressCircle.postDelayed(hideCircle, 100)
          //stat.setVisibility(View.GONE)
      }
      state = bgService.getState
    }
  }

  protected override def onResume() {
    super.onResume()

    app.refreshContainerHolder()

    updateState()
  }

  override def onStart() {
    super.onStart()
    registerCallback()
  }

  override def onBackPressed(): Unit =
    if (currentFragment != profilesFragment) displayFragment(profilesFragment) else super.onBackPressed()

  override def onStop() {
    super.onStop()
    unregisterCallback()
  }

  override def onDestroy() {
    super.onDestroy()
    unregisterReceiver(connectedListener)
    app.settings.unregisterOnSharedPreferenceChangeListener(this)
    detachService()
    new BackupManager(this).dataChanged()
    handler.removeCallbacksAndMessages(null)
  }

  override def onActivityResult(requestCode: Int, resultCode: Int, data: Intent): Unit = resultCode match {
    case Activity.RESULT_OK =>
      serviceLoad()
    case _ =>
      changeSwitch(checked = false)
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
}
