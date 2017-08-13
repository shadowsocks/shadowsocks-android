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

import java.lang.System.currentTimeMillis
import java.net.{HttpURLConnection, URL}
import java.util.Locale

import android.app.backup.BackupManager
import android.app.{Activity, ProgressDialog}
import android.content._
import android.net.{Uri, VpnService}
import android.nfc.{NdefMessage, NfcAdapter}
import android.os.{Build, Bundle, Handler, Message}
import android.support.customtabs.CustomTabsIntent
import android.support.design.widget.{FloatingActionButton, Snackbar}
import android.support.v4.content.ContextCompat
import android.support.v7.app.AlertDialog
import android.support.v7.content.res.AppCompatResources
import android.support.v7.preference.PreferenceDataStore
import android.support.v7.widget.RecyclerView.ViewHolder
import android.text.TextUtils
import android.util.Log
import android.view.View
import android.widget.{TextView, Toast}
import com.github.jorgecastilloprz.FABProgressCircle
import com.github.shadowsocks.ShadowsocksApplication.app
import com.github.shadowsocks.acl.{Acl, CustomRulesFragment}
import com.github.shadowsocks.aidl.IShadowsocksServiceCallback
import com.github.shadowsocks.preference.OnPreferenceDataStoreChangeListener
import com.github.shadowsocks.utils.CloseUtils.autoDisconnect
import com.github.shadowsocks.utils._
import com.mikepenz.crossfader.Crossfader
import com.mikepenz.crossfader.view.CrossFadeSlidingPaneLayout
import com.mikepenz.materialdrawer.interfaces.ICrossfader
import com.mikepenz.materialdrawer.model.PrimaryDrawerItem
import com.mikepenz.materialdrawer.model.interfaces.IDrawerItem
import com.mikepenz.materialdrawer.{Drawer, DrawerBuilder}

object MainActivity {
  private final val TAG = "ShadowsocksMainActivity"
  private final val REQUEST_CONNECT = 1

  private final val DRAWER_PROFILES = 0L
  private final val DRAWER_GLOBAL_SETTINGS = 1L
  private final val DRAWER_RECOVERY = 2L
  private final val DRAWER_ABOUT = 3L
  private final val DRAWER_FAQ = 4L
  private final val DRAWER_CUSTOM_RULES = 5L
}

class MainActivity extends Activity with ServiceBoundContext with Drawer.OnDrawerItemClickListener
  with OnPreferenceDataStoreChangeListener {
  import MainActivity._

  // UI
  private val handler = new Handler()
  private var fab: FloatingActionButton = _
  private var fabProgressCircle: FABProgressCircle = _
  var crossfader: Crossfader[CrossFadeSlidingPaneLayout] = _
  var drawer: Drawer = _

  private var testCount: Int = _
  private var statusText: TextView = _
  private var txText: TextView = _
  private var rxText: TextView = _
  private var txRateText: TextView = _
  private var rxRateText: TextView = _

  private lazy val customTabsIntent = new CustomTabsIntent.Builder()
    .setToolbarColor(ContextCompat.getColor(this, R.color.material_primary_500))
    .build()
  def launchUrl(url: String): Unit = try customTabsIntent.launchUrl(this, Uri.parse(url)) catch {
    case _: ActivityNotFoundException => // Ignore
  }

  // Services
  var state: Int = _
  private val callback = new IShadowsocksServiceCallback.Stub {
    def stateChanged(s: Int, profileName: String, m: String): Unit = handler.post(() => changeState(s, profileName, m))
    def trafficUpdated(profileId: Int, txRate: Long, rxRate: Long, txTotal: Long, rxTotal: Long): Unit =
      handler.post(() => updateTraffic(profileId, txRate, rxRate, txTotal, rxTotal))
    override def trafficPersisted(profileId: Int): Unit = handler.post(() => if (ProfilesFragment.instance != null)
      ProfilesFragment.instance.onTrafficPersisted(profileId))
  }

  private lazy val greyTint = ContextCompat.getColorStateList(MainActivity.this, R.color.material_primary_500)
  private lazy val greenTint = ContextCompat.getColorStateList(MainActivity.this, R.color.material_green_700)
  private def hideCircle() = try fabProgressCircle.hide() catch {
    case _: NullPointerException =>
  }
  private def changeState(s: Int, profileName: String = null, m: String = null) {
    s match {
      case State.CONNECTING =>
        fab.setImageResource(R.drawable.ic_start_busy)
        fabProgressCircle.show()
        statusText.setText(R.string.connecting)
      case State.CONNECTED =>
        if (state == State.CONNECTING) fabProgressCircle.beginFinalAnimation()
        else fabProgressCircle.postDelayed(hideCircle, 1000)
        fab.setImageResource(R.drawable.ic_start_connected)
        statusText.setText(if (app.isNatEnabled) R.string.nat_connected else R.string.vpn_connected)
      case State.STOPPING =>
        fab.setImageResource(R.drawable.ic_start_busy)
        if (state == State.CONNECTED) fabProgressCircle.show()  // ignore for stopped
        statusText.setText(R.string.stopping)
      case _ =>
        fab.setImageResource(R.drawable.ic_start_idle)
        fabProgressCircle.postDelayed(hideCircle, 1000)
        if (m != null) {
          val snackbar = Snackbar.make(findViewById(R.id.snackbar),
            getString(R.string.vpn_error).formatLocal(Locale.ENGLISH, m), Snackbar.LENGTH_LONG)
          if (m == getString(R.string.nat_no_root)) addDisableNatToSnackbar(snackbar)
          snackbar.show()
          Log.e(TAG, "Error to start VPN service: " + m)
        }
        statusText.setText(R.string.not_connected)
    }
    state = s
    if (state == State.CONNECTED) fab.setBackgroundTintList(greenTint) else {
      fab.setBackgroundTintList(greyTint)
      updateTraffic(-1, 0, 0, 0, 0)
      testCount += 1  // suppress previous test messages
    }
    if (ProfilesFragment.instance != null)
      ProfilesFragment.instance.profilesAdapter.notifyDataSetChanged()  // refresh button enabled state
    fab.setEnabled(false)
    if (state == State.CONNECTED || state == State.STOPPED)
      handler.postDelayed(() => fab.setEnabled(state == State.CONNECTED || state == State.STOPPED), 1000)
  }
  def updateTraffic(profileId: Int, txRate: Long, rxRate: Long, txTotal: Long, rxTotal: Long) {
    txText.setText(TrafficMonitor.formatTraffic(txTotal))
    rxText.setText(TrafficMonitor.formatTraffic(rxTotal))
    txRateText.setText(TrafficMonitor.formatTraffic(txRate) + "/s")
    rxRateText.setText(TrafficMonitor.formatTraffic(rxRate) + "/s")
    val child = getFragmentManager.findFragmentById(R.id.fragment_holder).asInstanceOf[ToolbarFragment]
    if (state != State.STOPPING && child != null) child.onTrafficUpdated(profileId, txRate, rxRate, txTotal, rxTotal)
  }

  override def onServiceConnected() {
    changeState(bgService.getState)
    if (Build.VERSION.SDK_INT >= 21 && app.isNatEnabled) {
      val snackbar = Snackbar.make(findViewById(R.id.snackbar), R.string.nat_deprecated, Snackbar.LENGTH_LONG)
      addDisableNatToSnackbar(snackbar)
      snackbar.show()
    }
  }
  override def onServiceDisconnected(): Unit = changeState(State.IDLE)

  private def addDisableNatToSnackbar(snackbar: Snackbar) = snackbar.setAction(R.string.switch_to_vpn, (_ =>
    if (state == State.STOPPED) app.dataStore.isNAT = false): View.OnClickListener)

  override def binderDied(): Unit = handler.post(() => {
    detachService()
    app.crashRecovery()
    attachService(callback)
  })

  override def onActivityResult(requestCode: Int, resultCode: Int, data: Intent): Unit = resultCode match {
    case Activity.RESULT_OK => Utils.startSsService(this)
    case _ => Log.e(TAG, "Failed to start VpnService")
  }

  override def onCreate(savedInstanceState: Bundle) {
    super.onCreate(savedInstanceState)
    setContentView(R.layout.layout_main)
    val drawerBuilder = new DrawerBuilder()
      .withActivity(this)
      .withTranslucentStatusBar(true)
      .withHeader(R.layout.layout_header)
      .addDrawerItems(
        new PrimaryDrawerItem()
          .withIdentifier(DRAWER_PROFILES)
          .withName(R.string.profiles)
          .withIcon(AppCompatResources.getDrawable(this, R.drawable.ic_action_description))
          .withIconTintingEnabled(true),
        new PrimaryDrawerItem()
          .withIdentifier(DRAWER_CUSTOM_RULES)
          .withName(R.string.custom_rules)
          .withIcon(AppCompatResources.getDrawable(this, R.drawable.ic_action_assignment))
          .withIconTintingEnabled(true),
        new PrimaryDrawerItem()
          .withIdentifier(DRAWER_GLOBAL_SETTINGS)
          .withName(R.string.settings)
          .withIcon(AppCompatResources.getDrawable(this, R.drawable.ic_action_settings))
          .withIconTintingEnabled(true)
      )
      .addStickyDrawerItems(
        new PrimaryDrawerItem()
          .withIdentifier(DRAWER_FAQ)
          .withName(R.string.faq)
          .withIcon(AppCompatResources.getDrawable(this, R.drawable.ic_action_help_outline))
          .withIconTintingEnabled(true)
          .withSelectable(false),
        new PrimaryDrawerItem()
          .withIdentifier(DRAWER_RECOVERY)
          .withName(R.string.recovery)
          .withIcon(AppCompatResources.getDrawable(this, R.drawable.ic_navigation_refresh))
          .withIconTintingEnabled(true)
          .withSelectable(false),
        new PrimaryDrawerItem()
          .withIdentifier(DRAWER_ABOUT)
          .withName(R.string.about)
          .withIcon(AppCompatResources.getDrawable(this, R.drawable.ic_action_copyright))
          .withIconTintingEnabled(true)
      )
      .withOnDrawerItemClickListener(this)
      .withActionBarDrawerToggle(true)
      .withSavedInstance(savedInstanceState)
    val miniDrawerWidth = getResources.getDimension(R.dimen.material_mini_drawer_item)
    if (getResources.getDisplayMetrics.widthPixels >=
      getResources.getDimension(R.dimen.profile_item_max_width) + miniDrawerWidth) {
      drawer = drawerBuilder.withGenerateMiniDrawer(true).buildView()
      crossfader = new Crossfader[CrossFadeSlidingPaneLayout]()
      crossfader.withContent(findViewById(android.R.id.content))
        .withFirst(drawer.getSlider, getResources.getDimensionPixelSize(R.dimen.material_drawer_width))
        .withSecond(drawer.getMiniDrawer.build(this), miniDrawerWidth.toInt)
        .withSavedInstance(savedInstanceState)
        .build()
      if (getResources.getConfiguration.getLayoutDirection == View.LAYOUT_DIRECTION_RTL)
        crossfader.getCrossFadeSlidingPaneLayout.setShadowDrawableRight(
          AppCompatResources.getDrawable(this, R.drawable.material_drawer_shadow_right))
      else crossfader.getCrossFadeSlidingPaneLayout.setShadowDrawableLeft(
        AppCompatResources.getDrawable(this, R.drawable.material_drawer_shadow_left))
      drawer.getMiniDrawer.withCrossFader(new ICrossfader { // a wrapper is needed
        def isCrossfaded: Boolean = crossfader.isCrossFaded
        def crossfade(): Unit = crossfader.crossFade()
      })
    } else drawer = drawerBuilder.build()

    val header = drawer.getHeader
    val title = header.findViewById[TextView](R.id.drawer_title)
    val tf = Typefaces.get(this, "fonts/Iceland.ttf")
    if (tf != null) title.setTypeface(tf)

    if (savedInstanceState == null) displayFragment(new ProfilesFragment)
    statusText = findViewById(R.id.status).asInstanceOf[TextView]
    txText = findViewById(R.id.tx).asInstanceOf[TextView]
    txRateText = findViewById(R.id.txRate).asInstanceOf[TextView]
    rxText = findViewById(R.id.rx).asInstanceOf[TextView]
    rxRateText = findViewById(R.id.rxRate).asInstanceOf[TextView]
    findViewById[View](R.id.stat).setOnClickListener(_ => if (state == State.CONNECTED && app.isVpnEnabled) {
      testCount += 1
      statusText.setText(R.string.connection_test_testing)
      val id = testCount  // it would change by other code
      Utils.ThrowableFuture {
        // Based on: https://android.googlesource.com/platform/frameworks/base/+/master/services/core/java/com/android/server/connectivity/NetworkMonitor.java#640
        autoDisconnect(new URL("https", app.currentProfile.get.route match {
          case Acl.CHINALIST => "www.qualcomm.cn"
          case _ => "www.google.com"
        }, "/generate_204").openConnection()
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
            if (testCount == id) handler.post(() => if (success) statusText.setText(result) else {
              statusText.setText(R.string.connection_test_fail)
              Snackbar.make(findViewById(R.id.snackbar), result, Snackbar.LENGTH_LONG).show()
            })
          }
        }
      }
    })

    fab = findViewById(R.id.fab).asInstanceOf[FloatingActionButton]
    fabProgressCircle = findViewById(R.id.fabProgressCircle).asInstanceOf[FABProgressCircle]
    fab.setOnClickListener(_ => if (state == State.CONNECTED) Utils.stopSsService(this) else Utils.ThrowableFuture {
      if (app.isNatEnabled) Utils.startSsService(this) else {
        val intent = VpnService.prepare(this)
        if (intent != null) startActivityForResult(intent, REQUEST_CONNECT)
        else handler.post(() => onActivityResult(REQUEST_CONNECT, Activity.RESULT_OK, null))
      }
    })
    fab.setOnLongClickListener(_ => {
      Utils.positionToast(Toast.makeText(this, if (state == State.CONNECTED) R.string.stop else R.string.connect,
        Toast.LENGTH_SHORT), fab, getWindow, 0, getResources.getDimensionPixelOffset(R.dimen.margin_small)).show()
      true
    })

    changeState(State.IDLE) // reset everything to init state
    handler.post(() => attachService(callback))
    app.dataStore.registerChangeListener(this)

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
      Snackbar.make(findViewById(R.id.snackbar), R.string.profile_invalid_input, Snackbar.LENGTH_LONG).show()
      return
    }
    new AlertDialog.Builder(this)
      .setTitle(R.string.add_profile_dialog)
      .setPositiveButton(R.string.yes, ((_, _) =>
        profiles.foreach(app.profileManager.createProfile)): DialogInterface.OnClickListener)
      .setNegativeButton(R.string.no, null)
      .setMessage(profiles.mkString("\n"))
      .create()
      .show()
  }

  def onPreferenceDataStoreChanged(store: PreferenceDataStore, key: String): Unit = key match {
    case Key.isNAT => handler.post(() => {
      detachService()
      attachService(callback)
    })
    case _ =>
  }

  private def displayFragment(fragment: ToolbarFragment) {
    getFragmentManager.beginTransaction().replace(R.id.fragment_holder, fragment).commitAllowingStateLoss()
    drawer.closeDrawer()
  }

  override def onItemClick(view: View, position: Int, drawerItem: IDrawerItem[_, _ <: ViewHolder]): Boolean = {
    drawerItem.getIdentifier match {
      case DRAWER_PROFILES => displayFragment(new ProfilesFragment)
      case DRAWER_RECOVERY =>
        app.track("GlobalConfigFragment", "reset")
        Utils.stopSsService(this)
        val dialog = ProgressDialog.show(this, "", getString(R.string.recovering), true, false)
        val handler = new Handler {
          override def handleMessage(msg: Message): Unit = if (dialog.isShowing && !isDestroyed) dialog.dismiss()
        }
        Utils.ThrowableFuture {
          app.crashRecovery()
          app.copyAssets()
          handler.sendEmptyMessage(0)
        }
      case DRAWER_GLOBAL_SETTINGS => displayFragment(new GlobalSettingsFragment)
      case DRAWER_ABOUT =>
        app.track(TAG, "about")
        displayFragment(new AboutFragment)
      case DRAWER_FAQ => launchUrl(getString(R.string.faq_url))
      case DRAWER_CUSTOM_RULES => displayFragment(new CustomRulesFragment)
    }
    true  // unexpected cases will throw exception
  }

  protected override def onResume() {
    super.onResume()
    app.remoteConfig.fetch()
    state match {
      case State.STOPPING | State.CONNECTING =>
      case _ => hideCircle()
    }
  }

  override def onStart() {
    super.onStart()
    setListeningForBandwidth(true)
  }

  override def onBackPressed(): Unit = if (drawer.isDrawerOpen) drawer.closeDrawer() else {
    val currentFragment = getFragmentManager.findFragmentById(R.id.fragment_holder).asInstanceOf[ToolbarFragment]
    if (!currentFragment.onBackPressed())
      if (currentFragment.isInstanceOf[ProfilesFragment]) super.onBackPressed() else {
        displayFragment(new ProfilesFragment)
        drawer.setSelection(DRAWER_PROFILES)
      }
  }

  override def onStop() {
    setListeningForBandwidth(false)
    super.onStop()
  }

  override def onSaveInstanceState(outState: Bundle) {
    super.onSaveInstanceState(outState)
    drawer.saveInstanceState(outState)
    if (crossfader != null) crossfader.saveInstanceState(outState)
  }

  override def onDestroy() {
    super.onDestroy()
    app.dataStore.unregisterChangeListener(this)
    detachService()
    new BackupManager(this).dataChanged()
    handler.removeCallbacksAndMessages(null)
  }
}
