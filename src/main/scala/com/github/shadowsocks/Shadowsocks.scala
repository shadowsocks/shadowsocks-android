/*
 * Shadowsocks - A shadowsocks client for Android
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

import android.app.{Activity, AlertDialog, ProgressDialog}
import android.content._
import android.content.res.AssetManager
import android.graphics.Typeface
import android.os._
import android.preference.{Preference, PreferenceManager}
import android.util.Log
import android.view.{ViewGroup, Gravity, ViewParent, KeyEvent}
import android.widget._
import com.actionbarsherlock.view.Menu
import com.actionbarsherlock.view.MenuItem
import com.google.analytics.tracking.android.EasyTracker
import de.keyboardsurfer.android.widget.crouton.Crouton
import de.keyboardsurfer.android.widget.crouton.Style
import java.io.FileOutputStream
import java.io.IOException
import java.io.InputStream
import java.io.OutputStream
import java.util.Hashtable
import net.saik0.android.unifiedpreference.UnifiedPreferenceFragment
import net.saik0.android.unifiedpreference.UnifiedSherlockPreferenceActivity
import org.jraf.android.backport.switchwidget.Switch
import android.content.pm.{PackageInfo, PackageManager}
import android.net.{Uri, VpnService}
import android.webkit.{WebViewClient, WebView}
import android.app.backup.BackupManager
import scala.concurrent.ops._
import android.content.SharedPreferences.OnSharedPreferenceChangeListener
import com.google.ads.{AdRequest, AdSize, AdView}
import net.simonvt.menudrawer.MenuDrawer

import com.github.shadowsocks.database._
import scala.collection.mutable.ListBuffer
import com.github.shadowsocks.database.Category
import com.github.shadowsocks.database.Item
import com.github.shadowsocks.database.Profile

object Shadowsocks {

  val PREFS_NAME = "Shadowsocks"
  val PROXY_PREFS = Array(Key.proxy, Key.remotePort, Key.localPort, Key.sitekey, Key.encMethod)
  val FEATRUE_PREFS = Array(Key.isGFWList, Key.isGlobalProxy, Key.proxyedApps,
    Key.isTrafficStat, Key.isAutoConnect)
  val TAG = "Shadowsocks"
  val REQUEST_CONNECT = 1

  private var vpnEnabled = -1

  def isServiceStarted(context: Context): Boolean = {
    ShadowsocksService.isServiceStarted(context) || ShadowVpnService.isServiceStarted(context)
  }

  class ProxyFragment extends UnifiedPreferenceFragment {

    var receiver: BroadcastReceiver = null

    private def setPreferenceEnabled() {
      val state = getActivity.asInstanceOf[Shadowsocks].state
      val enabled = state != State.CONNECTED && state != State.CONNECTING
      for (name <- PROXY_PREFS) {
        val pref: Preference = findPreference(name)
        if (pref != null) {
          pref.setEnabled(enabled)
        }
      }
    }

    override def onCreate(bundle: Bundle) {
      super.onCreate(bundle)
      val filter = new IntentFilter()
      filter.addAction(Action.UPDATE_FRAGMENT)
      receiver = new BroadcastReceiver {
        def onReceive(p1: Context, p2: Intent) {
          setPreferenceEnabled()
        }
      }
      getActivity.getApplicationContext.registerReceiver(receiver, filter)
    }

    override def onDestroy() {
      super.onDestroy()
      getActivity.getApplicationContext.unregisterReceiver(receiver)
    }

    override def onResume() {
      super.onResume()
      setPreferenceEnabled()
    }

    override def onPause() {
      super.onPause()
    }
  }

  class FeatureFragment extends UnifiedPreferenceFragment with OnSharedPreferenceChangeListener {

    var receiver: BroadcastReceiver = null

    private def setPreferenceEnabled() {
      val state = getActivity.asInstanceOf[Shadowsocks].state
      val enabled: Boolean = state != State.CONNECTED && state != State.CONNECTING
      for (name <- Shadowsocks.FEATRUE_PREFS) {
        val pref: Preference = findPreference(name)
        if (pref != null) {
          val status = getActivity.getSharedPreferences(Key.status, Context.MODE_PRIVATE)
          val isRoot = status.getBoolean(Key.isRoot, false)
          if (Seq(Key.isAutoConnect, Key.isGlobalProxy, Key.isTrafficStat,
            Key.proxyedApps).contains(name)) {
            pref.setEnabled(enabled && isRoot)
          } else {
            pref.setEnabled(enabled)
          }
        }
      }
    }

    override def onCreate(bundle: Bundle) {
      super.onCreate(bundle)
      val filter = new IntentFilter()
      filter.addAction(Action.UPDATE_FRAGMENT)
      receiver = new BroadcastReceiver {
        def onReceive(p1: Context, p2: Intent) {
          setPreferenceEnabled()
        }
      }
      getActivity.getApplicationContext.registerReceiver(receiver, filter)
    }

    override def onDestroy() {
      super.onDestroy()
      getActivity.getApplicationContext.unregisterReceiver(receiver)
    }

    override def onResume() {
      super.onResume()
      setPreferenceEnabled()
    }

    override def onPause() {
      super.onPause()
    }

    def onSharedPreferenceChanged(prefs: SharedPreferences, key: String) {
      if (key == Key.isGlobalProxy) setPreferenceEnabled()
    }
  }

}

object Typefaces {
  def get(c: Context, assetPath: String): Typeface = {
    cache synchronized {
      if (!cache.containsKey(assetPath)) {
        try {
          val t: Typeface = Typeface.createFromAsset(c.getAssets, assetPath)
          cache.put(assetPath, t)
        } catch {
          case e: Exception => {
            Log.e(TAG, "Could not get typeface '" + assetPath + "' because " + e.getMessage)
            return null
          }
        }
      }
      return cache.get(assetPath)
    }
  }

  private final val TAG: String = "Typefaces"
  private final val cache: Hashtable[String, Typeface] = new Hashtable[String, Typeface]
}

class Shadowsocks
  extends UnifiedSherlockPreferenceActivity
  with CompoundButton.OnCheckedChangeListener
  with OnSharedPreferenceChangeListener {

  private val MSG_CRASH_RECOVER: Int = 1
  private val MSG_INITIAL_FINISH: Int = 2

  private var switchButton: Switch = null
  private var progressDialog: ProgressDialog = null

  private var state = State.INIT
  private var prepared = false

  lazy val settings = PreferenceManager.getDefaultSharedPreferences(this)
  lazy val status = getSharedPreferences(Key.status, Context.MODE_PRIVATE)
  lazy val receiver = new StateBroadcastReceiver
  lazy val drawer = MenuDrawer.attach(this)
  lazy val listView = new ListView(this)
  lazy val menuAdapter = new MenuAdapter(this, getMenuList)
  lazy val profileManager = getApplication.asInstanceOf[ShadowsocksApplication].profileManager

  private val handler: Handler = new Handler {
    override def handleMessage(msg: Message) {
      msg.what match {
        case MSG_CRASH_RECOVER =>
          Crouton.makeText(Shadowsocks.this, R.string.crash_alert, Style.ALERT).show()
          status.edit().putBoolean(Key.isRunning, false).commit()
        case MSG_INITIAL_FINISH =>
          clearDialog()
      }
      super.handleMessage(msg)
    }
  }

  private def copyAssets(path: String) {
    val assetManager: AssetManager = getAssets
    var files: Array[String] = null
    try {
      files = assetManager.list(path)
    } catch {
      case e: IOException => {
        Log.e(Shadowsocks.TAG, e.getMessage)
      }
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
          out = new FileOutputStream("/data/data/com.github.shadowsocks/" + file)
          copyFile(in, out)
          in.close()
          in = null
          out.flush()
          out.close()
          out = null
        } catch {
          case ex: Exception => {
            Log.e(Shadowsocks.TAG, ex.getMessage)
          }
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

  def onSharedPreferenceChanged(prefs: SharedPreferences, key: String) {
    if (key == Key.isGlobalProxy) {
      val enabled = state != State.CONNECTED && state != State.CONNECTING
      setPreferenceEnabled(enabled)
    }
  }

  private def crash_recovery() {
    val sb = new StringBuilder

    sb.append("kill -9 `cat /data/data/com.github.shadowsocks/pdnsd.pid`").append("\n")
    sb.append("kill -9 `cat /data/data/com.github.shadowsocks/shadowsocks.pid`").append("\n")
    sb.append("kill -9 `cat /data/data/com.github.shadowsocks/tun2socks.pid`").append("\n")
    sb.append("killall -9 pdnsd").append("\n")
    sb.append("killall -9 shadowsocks").append("\n")
    sb.append("killall -9 tun2socks").append("\n")
    sb.append("rm /data/data/com.github.shadowsocks/pdnsd.conf").append("\n")
    Utils.runCommand(sb.toString())

    sb.clear()
    sb.append("kill -9 `cat /data/data/com.github.shadowsocks/redsocks.pid`").append("\n")
    sb.append("killall -9 redsocks").append("\n")
    sb.append("rm /data/data/com.github.shadowsocks/redsocks.conf").append("\n")
    sb.append(Utils.getIptables).append(" -t nat -F OUTPUT").append("\n")
    Utils.runRootCommand(sb.toString())
  }

  private def getVersionName: String = {
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

  private def isTextEmpty(s: String, msg: String): Boolean = {
    if (s == null || s.length <= 0) {
      showDialog(msg)
      return true
    }
    false
  }

  def prepareStartService() {
    clearDialog()
    progressDialog = ProgressDialog
      .show(Shadowsocks.this, "", getString(R.string.connecting), true, true)
    spawn {
      if (isVpnEnabled) {
        val intent = VpnService.prepare(this)
        if (intent != null) {
          startActivityForResult(intent, Shadowsocks.REQUEST_CONNECT)
        } else {
          onActivityResult(Shadowsocks.REQUEST_CONNECT, Activity.RESULT_OK, null)
        }
      } else {
        if (!serviceStart) {
          switchButton.setChecked(false)
        }
      }
    }
  }

  def onCheckedChanged(compoundButton: CompoundButton, checked: Boolean) {
    if (compoundButton eq switchButton) {
      checked match {
        case true => {
          prepareStartService()
        }
        case false => {
          serviceStop()
        }
      }
    }
  }

  def getLayoutView(view: ViewParent): LinearLayout = {
    view match {
      case layout: LinearLayout => layout
      case _ => if (view != null) getLayoutView(view.getParent) else null
    }
  }

  def initAdView(layoutResId: Int) {
    if (settings.getString(Key.proxy, "") == "198.199.101.152") {
      val adView = {
        if (isSinglePane) {
          new AdView(this, AdSize.SMART_BANNER, "a151becb8068b09")
        } else {
          new AdView(this, AdSize.BANNER, "a151becb8068b09")
        }
      }
      val layoutView = findViewById(layoutResId).asInstanceOf[ViewGroup]
      if (layoutView != null) {
        layoutView.addView(adView, 0)
        adView.loadAd(new AdRequest)
      }
    }
  }

  override def setContentView(layoutResId: Int) {
    drawer.setContentView(layoutResId)
    initAdView(layoutResId)
    onContentChanged()
  }

  /** Called when the activity is first created. */
  override def onCreate(savedInstanceState: Bundle) {
    setHeaderRes(R.xml.shadowsocks_headers)
    super.onCreate(savedInstanceState)

    listView.setAdapter(menuAdapter)
    drawer.setMenuView(listView)

    // The drawable that replaces the up indicator in the action bar
    drawer.setSlideDrawable(R.drawable.ic_drawer)
    // Whether the previous drawable should be shown
    drawer.setDrawerIndicatorEnabled(true)

    val switchLayout = getLayoutInflater
      .inflate(R.layout.layout_switch, null)
      .asInstanceOf[RelativeLayout]
    getSupportActionBar.setCustomView(switchLayout)
    getSupportActionBar.setDisplayShowTitleEnabled(false)
    getSupportActionBar.setDisplayShowCustomEnabled(true)
    // getSupportActionBar.setDisplayShowHomeEnabled(false)

    val title: TextView = switchLayout.findViewById(R.id.title).asInstanceOf[TextView]
    val tf: Typeface = Typefaces.get(this, "fonts/Iceland.ttf")
    if (tf != null) title.setTypeface(tf)
    title.setText(R.string.app_name)

    switchButton = switchLayout.findViewById(R.id.switchButton).asInstanceOf[Switch]
    registerReceiver(receiver, new IntentFilter(Action.UPDATE_STATE))

    val init: Boolean = !Shadowsocks.isServiceStarted(this)
    if (init) {
      if (progressDialog == null) {
        progressDialog = ProgressDialog.show(this, "", getString(R.string.initializing), true, true)
      }
      spawn {
        status.edit().putBoolean(Key.isRoot, Utils.getRoot).commit()
        if (!status.getBoolean(getVersionName, false)) {
          status.edit.putBoolean(getVersionName, true).apply()
          reset()
        }
        handler.sendEmptyMessage(MSG_INITIAL_FINISH)
      }
    }

  }

  def updateProfile(id: Int) {

  }

  def getProfileList: List[Item] = {
    val list = profileManager.getAllProfiles getOrElse List[Profile]()
    list.map(p => new Item(p.id, p.name, -1, updateProfile))
  }

  def getMenuList: List[Any] = {

    val buf = new ListBuffer[Any]()

    buf += new Category("Profiles")

    buf ++= getProfileList

    buf += new Category("Settings")

    buf += new Item(-1, getString(R.string.recovery), android.R.drawable.ic_menu_revert,
      _ => {
        EasyTracker.getTracker.sendEvent(Shadowsocks.TAG, "reset", getVersionName, 0L)
        recovery()
      })

    buf += new Item(-2, getString(R.string.flush_dnscache), android.R.drawable.ic_menu_delete,
      _ => {
        EasyTracker.getTracker.sendEvent(Shadowsocks.TAG, "flush_dnscache", getVersionName, 0L)
        flushDnsCache()
      })

    buf += new Item(-3, getString(R.string.about), android.R.drawable.ic_menu_info_details,
      _ => {
        EasyTracker.getTracker.sendEvent(Shadowsocks.TAG, "about", getVersionName, 0L)
        showAbout()
      })

    buf.toList

  }

  override def onKeyDown(keyCode: Int, event: KeyEvent): Boolean = {
    if (keyCode == KeyEvent.KEYCODE_BACK && event.getRepeatCount == 0) {
      try {
        finish()
      } catch {
        case ignore: Exception => {
        }
      }
      return true
    }
    super.onKeyDown(keyCode, event)
  }

  override def onOptionsItemSelected(item: MenuItem): Boolean = {
    item.getItemId match {
      case android.R.id.home => {
        EasyTracker.getTracker.sendEvent(Shadowsocks.TAG, "home", getVersionName, 0L)
        drawer.toggleMenu()
        return true
      }
    }
    super.onOptionsItemSelected(item)
  }

  protected override def onPause() {
    super.onPause()
    prepared = false
  }

  protected override def onResume() {
    super.onResume()
    if (!prepared) {
      if (Shadowsocks.isServiceStarted(this)) {
        switchButton.setChecked(true)
        if (ShadowVpnService.isServiceStarted(this)) {
          val style = new Style.Builder()
            .setBackgroundColorValue(Style.holoBlueLight)
            .setDuration(Style.DURATION_INFINITE)
            .build()
          switchButton.setEnabled(false)
          Crouton.makeText(Shadowsocks.this, R.string.vpn_status, style).show()
        }
        setPreferenceEnabled(enabled = false)
        onStateChanged(State.CONNECTED, null)
      } else {
        switchButton.setEnabled(true)
        switchButton.setChecked(false)
        Crouton.cancelAllCroutons()
        setPreferenceEnabled(enabled = true)
        if (status.getBoolean(Key.isRunning, false)) {
          spawn {
            crash_recovery()
            handler.sendEmptyMessage(MSG_CRASH_RECOVER)
          }
        }
        onStateChanged(State.STOPPED, null)
      }
    }

    switchButton.setOnCheckedChangeListener(this)
  }

  private def setPreferenceEnabled(enabled: Boolean) {
    val isRoot = status.getBoolean(Key.isRoot, false)
    for (name <- Shadowsocks.PROXY_PREFS) {
      val pref = findPreference(name)
      if (pref != null) {
        pref.setEnabled(enabled)
      }
    }
    for (name <- Shadowsocks.FEATRUE_PREFS) {
      val pref = findPreference(name)
      if (pref != null) {
        if (Seq(Key.isAutoConnect, Key.isGlobalProxy, Key.isTrafficStat,
          Key.proxyedApps).contains(name)) {
          pref.setEnabled(enabled && isRoot)
        } else {
          pref.setEnabled(enabled)
        }
      }
    }
  }

  override def onStart() {
    super.onStart()
    EasyTracker.getInstance.activityStart(this)
  }

  override def onStop() {
    super.onStop()
    EasyTracker.getInstance.activityStop(this)
    clearDialog()
  }

  override def onDestroy() {
    super.onDestroy()
    Crouton.cancelAllCroutons()
    unregisterReceiver(receiver)
    new BackupManager(this).dataChanged()
  }

  def reset() {
    crash_recovery()
    copyAssets(Utils.getABI)
    Utils.runCommand("chmod 755 /data/data/com.github.shadowsocks/iptables\n"
      + "chmod 755 /data/data/com.github.shadowsocks/redsocks\n"
      + "chmod 755 /data/data/com.github.shadowsocks/pdnsd\n"
      + "chmod 755 /data/data/com.github.shadowsocks/shadowsocks\n"
      + "chmod 755 /data/data/com.github.shadowsocks/tun2socks\n")
  }

  private def recovery() {
    clearDialog()
    progressDialog = ProgressDialog.show(this, "", getString(R.string.recovering), true, true)
    val h: Handler = new Handler {
      override def handleMessage(msg: Message) {
        clearDialog()
      }
    }
    serviceStop()
    spawn {
      reset()
      h.sendEmptyMessage(0)
    }
  }

  private def flushDnsCache() {
    clearDialog()
    progressDialog = ProgressDialog.show(this, "", getString(R.string.flushing), true, true)
    val h: Handler = new Handler {
      override def handleMessage(msg: Message) {
        clearDialog()
      }
    }
    spawn {
      Utils.toggleAirplaneMode(getBaseContext)
      h.sendEmptyMessage(0)
    }
  }

  override def onActivityResult(requestCode: Int, resultCode: Int, data: Intent) {
    resultCode match {
      case Activity.RESULT_OK => {
        prepared = true
        if (!serviceStart) {
          switchButton.setChecked(false)
        }
      }
      case _ => {
        clearDialog()
        Log.e(Shadowsocks.TAG, "Failed to start VpnService")
      }
    }
  }

  def isVpnEnabled: Boolean = {
    if (Shadowsocks.vpnEnabled < 0) {
      Shadowsocks.vpnEnabled = if (Build.VERSION.SDK_INT
        >= Build.VERSION_CODES.ICE_CREAM_SANDWICH && !Utils.getRoot) {
        1
      } else {
        0
      }
    }
    if (Shadowsocks.vpnEnabled == 1) true else false
  }

  def serviceStop() {
    sendBroadcast(new Intent(Action.CLOSE))
  }

  /** Called when connect button is clicked. */
  def serviceStart: Boolean = {

    val proxy = settings.getString(Key.proxy, "")
    if (isTextEmpty(proxy, getString(R.string.proxy_empty))) return false
    val portText = settings.getString(Key.localPort, "")
    if (isTextEmpty(portText, getString(R.string.port_empty))) return false
    try {
      val port: Int = Integer.valueOf(portText)
      if (port <= 1024) {
        this.showDialog(getString(R.string.port_alert))
        return false
      }
    } catch {
      case ex: Exception => {
        this.showDialog(getString(R.string.port_alert))
        return false
      }
    }

    if (isVpnEnabled) {
      if (ShadowVpnService.isServiceStarted(this)) return false
      val intent: Intent = new Intent(this, classOf[ShadowVpnService])
      Extra.put(settings, intent)
      startService(intent)
      val style = new Style.Builder()
        .setBackgroundColorValue(Style.holoBlueLight)
        .setDuration(Style.DURATION_INFINITE)
        .build()
      Crouton.makeText(Shadowsocks.this, R.string.vpn_status, style).show()
      switchButton.setEnabled(false)
    } else {
      if (ShadowsocksService.isServiceStarted(this)) return false
      val intent: Intent = new Intent(this, classOf[ShadowsocksService])
      Extra.put(settings, intent)
      startService(intent)
    }
    true
  }

  private def showAbout() {

    val web = new WebView(this)
    web.loadUrl("file:///android_asset/pages/about.html")
    web.setWebViewClient(new WebViewClient() {
      override def shouldOverrideUrlLoading(view: WebView, url: String): Boolean = {
        startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(url)))
        true
      }
    })

    var versionName = ""
    try {
      versionName = getPackageManager.getPackageInfo(getPackageName, 0).versionName
    } catch {
      case ex: PackageManager.NameNotFoundException => {
        versionName = ""
      }
    }

    new AlertDialog.Builder(this)
      .setTitle(getString(R.string.about_title).format(versionName))
      .setCancelable(false)
      .setNegativeButton(getString(R.string.ok_iknow), new DialogInterface.OnClickListener() {
      override def onClick(dialog: DialogInterface, id: Int) {
        dialog.cancel()
      }
    })
      .setView(web)
      .create()
      .show()
  }

  private def showDialog(msg: String) {
    val builder: AlertDialog.Builder = new AlertDialog.Builder(this)
    builder
      .setMessage(msg)
      .setCancelable(false)
      .setNegativeButton(getString(R.string.ok_iknow), new DialogInterface.OnClickListener {
      def onClick(dialog: DialogInterface, id: Int) {
        dialog.cancel()
      }
    })
    val alert: AlertDialog = builder.create
    alert.show()
  }

  def clearDialog() {
    if (progressDialog != null) {
      progressDialog.dismiss()
      progressDialog = null
    }
  }

  def onStateChanged(s: Int, m: String) {
    if (state != s) {
      state = s
      state match {
        case State.CONNECTING => {
          if (progressDialog == null) {
            progressDialog = ProgressDialog
              .show(Shadowsocks.this, "", getString(R.string.connecting), true, true)
          }
          setPreferenceEnabled(enabled = false)
        }
        case State.CONNECTED => {
          clearDialog()
          if (!switchButton.isChecked) switchButton.setChecked(true)
          setPreferenceEnabled(enabled = false)
        }
        case State.STOPPED => {
          clearDialog()
          if (switchButton.isChecked) {
            switchButton.setEnabled(true)
            switchButton.setChecked(false)
            Crouton.cancelAllCroutons()
          }
          if (m != null) {
            Crouton.cancelAllCroutons()
            val style = new Style.Builder()
              .setBackgroundColorValue(Style.holoRedLight)
              .setDuration(Style.DURATION_INFINITE)
              .build()
            Crouton
              .makeText(Shadowsocks.this, getString(R.string.vpn_error).format(m), style)
              .show()
          }
          setPreferenceEnabled(enabled = true)
        }
      }
      sendBroadcast(new Intent(Action.UPDATE_FRAGMENT))
    }
  }

  class StateBroadcastReceiver extends BroadcastReceiver {
    override def onReceive(context: Context, intent: Intent) {
      val state = intent.getIntExtra(Extra.STATE, State.INIT)
      val message = intent.getStringExtra(Extra.MESSAGE)
      onStateChanged(state, message)
    }
  }

}
