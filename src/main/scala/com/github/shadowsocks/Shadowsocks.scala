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

import java.io.{ByteArrayInputStream, ByteArrayOutputStream, FileOutputStream, IOException, InputStream, OutputStream}
import java.util
import java.util.Locale

import android.app.backup.BackupManager
import android.app.{Activity, AlertDialog, ProgressDialog}
import android.content._
import android.content.pm.{PackageInfo, PackageManager}
import android.content.res.AssetManager
import android.graphics.{Bitmap, Color, Typeface}
import android.net.{Uri, VpnService}
import android.os._
import android.preference.{Preference, PreferenceManager, SwitchPreference}
import android.support.v4.content.ContextCompat
import android.support.design.widget.{FloatingActionButton, Snackbar}
import android.support.v7.app.AppCompatActivity
import android.support.v7.widget.Toolbar
import android.util.{DisplayMetrics, Log}
import android.view.{View, ViewGroup, ViewParent}
import android.webkit.{WebView, WebViewClient}
import android.widget._
import com.github.jorgecastilloprz.FABProgressCircle
import com.github.shadowsocks.aidl.{IShadowsocksService, IShadowsocksServiceCallback}
import com.github.shadowsocks.database._
import com.github.shadowsocks.preferences.{DropDownPreference, PasswordEditTextPreference, ProfileEditTextPreference, SummaryEditTextPreference}
import com.github.shadowsocks.utils._
import com.google.android.gms.ads.{AdRequest, AdSize, AdView}
import com.google.android.gms.analytics.HitBuilders
import com.google.zxing.integration.android.IntentIntegrator
import com.joanzapata.android.iconify.IconDrawable
import com.joanzapata.android.iconify.Iconify.IconValue
import com.nostra13.universalimageloader.core.download.BaseImageDownloader
import net.glxn.qrgen.android.QRCode
import net.simonvt.menudrawer.MenuDrawer

import scala.collection.mutable.{ArrayBuffer, ListBuffer}
import scala.concurrent.ExecutionContext.Implicits.global
import scala.concurrent.Future

class ProfileIconDownloader(context: Context, connectTimeout: Int, readTimeout: Int)
  extends BaseImageDownloader(context, connectTimeout, readTimeout) {

  def this(context: Context) {
    this(context, 0, 0)
  }

  override def getStreamFromOtherSource(imageUri: String, extra: AnyRef): InputStream = {
    val text = imageUri.substring(Scheme.PROFILE.length)
    val size = Utils.dpToPx(context, 16).toInt
    val idx = text.getBytes.last % 6
    val color = Seq(Color.MAGENTA, Color.GREEN, Color.YELLOW, Color.BLUE, Color.DKGRAY, Color.CYAN)(
      idx)
    val bitmap = Utils.getBitmap(text, size, size, color)

    val os = new ByteArrayOutputStream()
    bitmap.compress(Bitmap.CompressFormat.PNG, 100, os)
    new ByteArrayInputStream(os.toByteArray)
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

  val PREFS_NAME = "Shadowsocks"
  val PROXY_PREFS = Array(Key.profileName, Key.proxy, Key.remotePort, Key.localPort, Key.sitekey,
    Key.encMethod)
  val FEATRUE_PREFS = Array(Key.route, Key.isGlobalProxy, Key.proxyedApps,
    Key.isUdpDns, Key.isAuth, Key.isIpv6, Key.isAutoConnect)

  val EXECUTABLES = Array(Executable.PDNSD, Executable.REDSOCKS, Executable.SS_TUNNEL, Executable.SS_LOCAL, Executable.TUN2SOCKS)

  // Helper functions
  def updateDropDownPreference(pref: Preference, value: String) {
    pref.asInstanceOf[DropDownPreference].setValue(value)
  }

  def updatePasswordEditTextPreference(pref: Preference, value: String) {
    pref.setSummary(value)
    pref.asInstanceOf[PasswordEditTextPreference].setText(value)
  }

  def updateSummaryEditTextPreference(pref: Preference, value: String) {
    pref.setSummary(value)
    pref.asInstanceOf[SummaryEditTextPreference].setText(value)
  }

  def updateProfileEditTextPreference(pref: Preference, value: String) {
    pref.asInstanceOf[ProfileEditTextPreference].resetSummary(value)
    pref.asInstanceOf[ProfileEditTextPreference].setText(value)
  }

  def updateSwitchPreference(pref: Preference, value: Boolean) {
    pref.asInstanceOf[SwitchPreference].setChecked(value)
  }

  def updatePreference(pref: Preference, name: String, profile: Profile) {
    name match {
      case Key.profileName => updateProfileEditTextPreference(pref, profile.name)
      case Key.proxy => updateSummaryEditTextPreference(pref, profile.host)
      case Key.remotePort => updateSummaryEditTextPreference(pref, profile.remotePort.toString)
      case Key.localPort => updateSummaryEditTextPreference(pref, profile.localPort.toString)
      case Key.sitekey => updatePasswordEditTextPreference(pref, profile.password)
      case Key.encMethod => updateDropDownPreference(pref, profile.method)
      case Key.route => updateDropDownPreference(pref, profile.route)
      case Key.isGlobalProxy => updateSwitchPreference(pref, profile.global)
      case Key.isUdpDns => updateSwitchPreference(pref, profile.udpdns)
      case Key.isAuth => updateSwitchPreference(pref, profile.auth)
      case Key.isIpv6 => updateSwitchPreference(pref, profile.ipv6)
      case _ =>
    }
  }
}

class Shadowsocks
  extends AppCompatActivity
  with MenuAdapter.MenuListener {

  // Flags
  val MSG_CRASH_RECOVER: Int = 1
  val STATE_MENUDRAWER = "com.github.shadowsocks.menuDrawer"
  val STATE_ACTIVE_VIEW_ID = "com.github.shadowsocks.activeViewId"
  var singlePane: Int = -1

  // Variables
  private var serviceStarted = false
  var fab: FloatingActionButton = _
  var fabProgressCircle: FABProgressCircle = _
  var progressDialog: ProgressDialog = _
  var progressTag = -1
  var state = State.INIT
  var prepared = false
  var currentProfile = new Profile
  var vpnEnabled = -1

  // Services
  var currentServiceName = classOf[ShadowsocksNatService].getName
  var bgService: IShadowsocksService = null
  val callback = new IShadowsocksServiceCallback.Stub {
    override def stateChanged(state: Int, msg: String) {
      onStateChanged(state, msg)
    }
  }
  val connection = new ServiceConnection {
    override def onServiceConnected(name: ComponentName, service: IBinder) {
      // Initialize the background service
      bgService = IShadowsocksService.Stub.asInterface(service)
      try {
        bgService.registerCallback(callback)
      } catch {
        case ignored: RemoteException => // Nothing
      }
      // Update the UI
      if (fab != null) fab.setEnabled(true)
      stateUpdate()

      if (!status.getBoolean(getVersionName, false)) {
        status.edit.putBoolean(getVersionName, true).apply()
        recovery()
      }
    }

    override def onServiceDisconnected(name: ComponentName) {
      if (fab != null) fab.setEnabled(false)
      try {
        if (bgService != null) bgService.unregisterCallback(callback)
      } catch {
        case ignored: RemoteException => // Nothing
      }
      bgService = null
    }
  }

  private lazy val preferences =
    getFragmentManager.findFragmentById(android.R.id.content).asInstanceOf[ShadowsocksSettings]
  lazy val settings = PreferenceManager.getDefaultSharedPreferences(this)
  lazy val status = getSharedPreferences(Key.status, Context.MODE_PRIVATE)
  private lazy val preferenceReceiver = new PreferenceBroadcastReceiver
  private lazy val drawer = MenuDrawer.attach(this)
  private lazy val menuAdapter = new MenuAdapter(this, getMenuList)
  private lazy val listView = new ListView(this)
  private lazy val profileManager =
    new ProfileManager(settings, getApplication.asInstanceOf[ShadowsocksApplication].dbHelper)
  private lazy val application = getApplication.asInstanceOf[ShadowsocksApplication]
  private lazy val greyTint = ContextCompat.getColorStateList(this, R.color.material_blue_grey_700)
  private lazy val greenTint = ContextCompat.getColorStateList(this, R.color.material_green_600)

  var handler = new Handler()

  def isSinglePane: Boolean = {
    if (singlePane == -1) {
      val metrics = new DisplayMetrics()
      getWindowManager.getDefaultDisplay.getMetrics(metrics)
      val widthPixels = metrics.widthPixels
      val scaleFactor = metrics.density
      val widthDp = widthPixels / scaleFactor

      singlePane = if (widthDp <= 720) 1 else 0
    }
    singlePane == 1
  }

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
    Console.runRootCommand(cmd.toArray)
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
    Console.runCommand(cmd.toArray)
    Console.runRootCommand(cmd.toArray)
    Console.runRootCommand(Utils.getIptables + " -t nat -F OUTPUT")
  }

  private def getVersionName: String = {
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

  def isTextEmpty(s: String, msg: String): Boolean = {
    if (s != null && s.length > 0) return false
    Snackbar.make(getWindow.getDecorView.findViewById(android.R.id.content), msg, Snackbar.LENGTH_LONG).show
    true
  }

  def cancelStart() {
    clearDialog()
    changeSwitch(checked = false)
  }

  def isReady: Boolean = {
    if (!checkText(Key.proxy)) return false
    if (!checkText(Key.sitekey)) return false
    if (!checkNumber(Key.localPort, low = false)) return false
    if (!checkNumber(Key.remotePort, low = true)) return false
    if (bgService == null) return false
    true
  }

  def prepareStartService() {
    Future {
      if (isVpnEnabled) {
        val intent = VpnService.prepare(this)
        if (intent != null) {
          startActivityForResult(intent, Shadowsocks.REQUEST_CONNECT)
        } else {
          onActivityResult(Shadowsocks.REQUEST_CONNECT, Activity.RESULT_OK, null)
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

  def initAdView() {
    if (settings.getString(Key.proxy, "") == "198.199.101.152") {
      val layoutView = {
        if (Build.VERSION.SDK_INT > 10) {
          drawer.getContentContainer.getChildAt(0)
        } else {
          getLayoutView(drawer.getContentContainer.getParent)
        }
      }
      if (layoutView != null) {
        val adView = new AdView(this)
        adView.setAdUnitId("ca-app-pub-9097031975646651/7760346322")
        adView.setAdSize(AdSize.SMART_BANNER)
        layoutView.asInstanceOf[ViewGroup].addView(adView, 0)
        adView.loadAd(new AdRequest.Builder().build())
      }
    }
  }

  override def setContentView(layoutResId: Int) {
    drawer.setContentView(layoutResId)
    initAdView()
    onContentChanged()
  }

  override def onCreate(savedInstanceState: Bundle) {

    super.onCreate(savedInstanceState)

    setContentView(R.layout.layout_main)

    // Update the profile
    if (!status.getBoolean(getVersionName, false)) {
      currentProfile = profileManager.create()
    }

    // Initialize the profile
    currentProfile = {
      profileManager.getProfile(settings.getInt(Key.profileId, -1)) getOrElse currentProfile
    }

    // Initialize drawer
    menuAdapter.setActiveId(settings.getInt(Key.profileId, -1))
    menuAdapter.setListener(this)
    listView.setAdapter(menuAdapter)
    drawer.setMenuView(listView)

    if (Utils.isLollipopOrAbove) {
      drawer.setDrawerIndicatorEnabled(false)
    } else {
      // The drawable that replaces the up indicator in the action bar
      drawer.setSlideDrawable(R.drawable.ic_drawer)
      // Whether the previous drawable should be shown
      drawer.setDrawerIndicatorEnabled(true)
    }

    if (!isSinglePane) {
      drawer.openMenu(false)
    }

    // Initialize action bar
    val toolbar = findViewById(R.id.toolbar).asInstanceOf[Toolbar]
    toolbar.setTitle(getTitle)
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
          if (fab.isEnabled) {
            fab.setEnabled(false)
            handler.postDelayed(() => {
              fab.setEnabled(true)
            }, 1000)
          }
      }
    })
    fab.setOnLongClickListener((v: View) => {
      Utils.positionToast(Toast.makeText(this, if (serviceStarted) R.string.stop else R.string.connect,
        Toast.LENGTH_SHORT), fab, getWindow, 0, Utils.dpToPx(this, 8).toInt).show
      true
    })
    toolbar.setNavigationIcon(R.drawable.ic_drawer)
    toolbar.setNavigationOnClickListener((v: View) => drawer.toggleMenu())
    title.setOnLongClickListener((v: View) => {
      if (Utils.isLollipopOrAbove && bgService != null
        && (bgService.getState == State.INIT || bgService.getState == State.STOPPED)) {
        val natEnabled = !status.getBoolean(Key.isNAT, !Utils.isLollipopOrAbove)
        status.edit().putBoolean(Key.isNAT, natEnabled).apply()
        deattachService()
        attachService()
        if (!Utils.isLollipopOrAbove) setPreferenceEnabled(State.isAvailable(bgService.getState))
        Toast.makeText(getBaseContext, if (natEnabled) R.string.enable_nat else R.string.disable_nat, Toast.LENGTH_LONG)
          .show()
        true
      } else {
        false
      }
    })

    // Register broadcast receiver
    registerReceiver(preferenceReceiver, new IntentFilter(Action.UPDATE_PREFS))

    // Bind to the service
    handler.post(() => {
      attachService()
    })
  }

  def attachService() {
    if (bgService == null) {
      val s = if (!isVpnEnabled) classOf[ShadowsocksNatService] else classOf[ShadowsocksVpnService]
      val intent = new Intent(this, s)
      intent.setAction(Action.SERVICE)
      bindService(intent, connection, Context.BIND_AUTO_CREATE)
      startService(new Intent(this, s))
    }
  }

  def deattachService() {
    if (bgService != null) {
      try {
        bgService.unregisterCallback(callback)
      } catch {
        case ignored: RemoteException => // Nothing
      }
      bgService = null
      unbindService(connection)
    }
  }

  override def onRestoreInstanceState(inState: Bundle) {
    super.onRestoreInstanceState(inState)
    drawer.restoreState(inState.getParcelable(STATE_MENUDRAWER))
  }

  override def onSaveInstanceState(outState: Bundle) {
    super.onSaveInstanceState(outState)
    outState.putParcelable(STATE_MENUDRAWER, drawer.saveState())
    outState.putInt(STATE_ACTIVE_VIEW_ID, currentProfile.id)
  }

  override def onBackPressed() {
    val drawerState = drawer.getDrawerState
    if (drawerState == MenuDrawer.STATE_OPEN || drawerState == MenuDrawer.STATE_OPENING) {
      drawer.closeMenu()
      return
    }
    super.onBackPressed()
  }

  override def onActiveViewChanged(v: View, pos: Int) {
    drawer.setActiveView(v, pos)
  }

  def newProfile(id: Int) {

    val builder = new AlertDialog.Builder(this)
    builder
      .setTitle(R.string.add_profile)
      .setItems(R.array.add_profile_methods, ((dialog: DialogInterface, which: Int) => which match {
        case 0 =>
          dialog.dismiss()
          val h = showProgress(R.string.loading)
          h.postDelayed(() => {
            val integrator = new IntentIntegrator(Shadowsocks.this)
            val list = new java.util.ArrayList(IntentIntegrator.TARGET_ALL_KNOWN)
            list.add("tw.com.quickmark")
            integrator.setTargetApplications(list)
            integrator.initiateScan()
            h.sendEmptyMessage(0)
          }, 600)
        case 1 =>
          dialog.dismiss()
          addProfile(id)
        case _ =>
      }): DialogInterface.OnClickListener)
    builder.create().show()
  }

  def reloadProfile() {
    drawer.closeMenu(true)

    val h = showProgress(R.string.loading)

    handler.postDelayed(() => {
      currentProfile = {
        profileManager.getProfile(settings.getInt(Key.profileId, -1)) getOrElse currentProfile
      }
      menuAdapter.updateList(getMenuList, currentProfile.id)

      updatePreferenceScreen()

      h.sendEmptyMessage(0)
    }, 600)
  }

  def addProfile(profile: Profile) {
    drawer.closeMenu(true)

    val h = showProgress(R.string.loading)

    handler.postDelayed(() => {
      currentProfile = profile
      profileManager.createOrUpdateProfile(currentProfile)
      profileManager.reload(currentProfile.id)
      menuAdapter.updateList(getMenuList, currentProfile.id)

      updatePreferenceScreen()

      h.sendEmptyMessage(0)
    }, 600)
  }

  def addProfile(id: Int) {
    drawer.closeMenu(true)

    val h = showProgress(R.string.loading)

    handler.postDelayed(() => {
      currentProfile = profileManager.reload(id)
      profileManager.save()
      menuAdapter.updateList(getMenuList, currentProfile.id)

      updatePreferenceScreen()

      h.sendEmptyMessage(0)
    }, 600)
  }

  def updateProfile(id: Int) {
    drawer.closeMenu(true)

    val h = showProgress(R.string.loading)

    handler.postDelayed(() => {
      currentProfile = profileManager.reload(id)
      menuAdapter.setActiveId(id)
      menuAdapter.notifyDataSetChanged()

      updatePreferenceScreen()

      h.sendEmptyMessage(0)
    }, 600)
  }

  def delProfile(id: Int): Boolean = {
    drawer.closeMenu(true)

    val profile = profileManager.getProfile(id)

    if (profile.isEmpty) return false

    new AlertDialog.Builder(this)
      .setMessage(String.format(Locale.ENGLISH, getString(R.string.remove_profile), profile.get.name))
      .setCancelable(false)
      .setNegativeButton(R.string.no,
        ((dialog: DialogInterface, i: Int) => dialog.cancel()): DialogInterface.OnClickListener)
      .setPositiveButton(R.string.yes, ((dialog: DialogInterface, i: Int) => {
        profileManager.delProfile(id)
        val profileId = {
          val profiles = profileManager.getAllProfiles.getOrElse(List[Profile]())
          if (profiles.isEmpty) -1 else profiles.head.id
        }
        currentProfile = profileManager.load(profileId)
        menuAdapter.updateList(getMenuList, currentProfile.id)

        updatePreferenceScreen()

        dialog.dismiss()
      }): DialogInterface.OnClickListener)
      .create()
      .show()

    true
  }

  def getProfileList: List[Item] = {
    val list = profileManager.getAllProfiles getOrElse List[Profile]()
    list.map(p => new IconItem(p.id, p.name, -1, updateProfile, delProfile))
  }

  def getMenuList: List[Any] = {

    val buf = new ListBuffer[Any]()

    buf += new Category(getString(R.string.profiles))

    buf ++= getProfileList

    buf +=
      new DrawableItem(-400, getString(R.string.add_profile), new IconDrawable(this, IconValue.fa_plus_circle)
        .colorRes(android.R.color.darker_gray).sizeDp(26), newProfile)

    buf += new Category(getString(R.string.settings))

    buf += new DrawableItem(-100, getString(R.string.recovery), new IconDrawable(this, IconValue.fa_recycle)
        .colorRes(android.R.color.darker_gray).sizeDp(26), _ => {
      // send event
      application.tracker.send(new HitBuilders.EventBuilder()
        .setCategory(Shadowsocks.TAG)
        .setAction("reset")
        .setLabel(getVersionName)
        .build())
      recovery()
    })

    buf +=
      new DrawableItem(-200, getString(R.string.flush_dnscache), new IconDrawable(this, IconValue.fa_refresh)
        .colorRes(android.R.color.darker_gray).sizeDp(26), _ => {
        // send event
        application.tracker.send(new HitBuilders.EventBuilder()
          .setCategory(Shadowsocks.TAG)
          .setAction("flush_dnscache")
          .setLabel(getVersionName)
          .build())
        flushDnsCache()
      })

    buf +=
      new DrawableItem(-300, getString(R.string.qrcode), new IconDrawable(this, IconValue.fa_qrcode)
        .colorRes(android.R.color.darker_gray).sizeDp(26), _ => {
        // send event
        application.tracker.send(new HitBuilders.EventBuilder()
          .setCategory(Shadowsocks.TAG)
          .setAction("qrcode")
          .setLabel(getVersionName)
          .build())
        showQrCode()
      })

    buf += new DrawableItem(-400, getString(R.string.about), new IconDrawable(this, IconValue.fa_info_circle)
        .colorRes(android.R.color.darker_gray).sizeDp(26), _ => {
      // send event
      application.tracker.send(new HitBuilders.EventBuilder()
        .setCategory(Shadowsocks.TAG)
        .setAction("about")
        .setLabel(getVersionName)
        .build())
      showAbout()
    })

    buf.toList
  }

  protected override def onPause() {
    super.onPause()
    prepared = false
  }

  private def stateUpdate() {
    if (bgService != null) {
      bgService.getState match {
        case State.CONNECTING =>
          fab.setBackgroundTintList(greyTint);
          fabProgressCircle.show()
          changeSwitch(checked = true)
        case State.CONNECTED =>
          fab.setBackgroundTintList(greenTint);
          fabProgressCircle.hide()
          changeSwitch(checked = true)
        case State.STOPPING =>
          fab.setBackgroundTintList(greyTint);
          fabProgressCircle.show()
          changeSwitch(checked = false)
        case _ =>
          fab.setBackgroundTintList(greyTint);
          fabProgressCircle.hide()
          changeSwitch(checked = false)
      }
      state = bgService.getState
    }
  }

  protected override def onResume() {
    super.onResume()
    stateUpdate()
    ConfigUtils.refresh(this)

    // Check if profile list changed
    val id = settings.getInt(Key.profileId, -1)
    if (id != -1 && id != currentProfile.id)
      reloadProfile()
  }

  private def setPreferenceEnabled(enabled: Boolean) {
    for (name <- Shadowsocks.PROXY_PREFS) {
      val pref = preferences.findPreference(name)
      if (pref != null) {
        pref.setEnabled(enabled)
      }
    }
    for (name <- Shadowsocks.FEATRUE_PREFS) {
      val pref = preferences.findPreference(name)
      if (pref != null) {
        if (Seq(Key.isGlobalProxy, Key.proxyedApps)
          .contains(name)) {
          pref.setEnabled(enabled && (Utils.isLollipopOrAbove || !isVpnEnabled))
        } else {
          pref.setEnabled(enabled)
        }
      }
    }
  }

  private def updatePreferenceScreen() {
    val profile = currentProfile
    for (name <- Shadowsocks.PROXY_PREFS) {
      val pref = preferences.findPreference(name)
      Shadowsocks.updatePreference(pref, name, profile)
    }
    for (name <- Shadowsocks.FEATRUE_PREFS) {
      val pref = preferences.findPreference(name)
      Shadowsocks.updatePreference(pref, name, profile)
    }
  }

  override def onStart() {
    super.onStart()
  }

  override def onStop() {
    super.onStop()
    clearDialog()
  }

  override def onDestroy() {
    super.onDestroy()
    deattachService()
    unregisterReceiver(preferenceReceiver)
    new BackupManager(this).dataChanged()
    handler.removeCallbacksAndMessages(null)
  }

  def copyToSystem() {
    val ab = new ArrayBuffer[String]
    ab.append("mount -o rw,remount -t yaffs2 /dev/block/mtdblock3 /system")
    for (executable <- Shadowsocks.EXECUTABLES) {
      ab.append("cp %s%s /system/bin/".formatLocal(Locale.ENGLISH, Path.BASE, executable))
      ab.append("chmod 755 /system/bin/" + executable)
      ab.append("chown root:shell /system/bin/" + executable)
    }
    ab.append("mount -o ro,remount -t yaffs2 /dev/block/mtdblock3 /system")
    Console.runRootCommand(ab.toArray)
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

  private def recovery() {
    serviceStop()
    val h = showProgress(R.string.recovering)
    Future {
      reset()
      h.sendEmptyMessage(0)
    }
  }

  private def dp2px(dp: Int): Int =
    Math.round(dp * (getBaseContext.getResources.getDisplayMetrics.xdpi / DisplayMetrics.DENSITY_DEFAULT))

  private def showQrCode() {
    val image = new ImageView(this)
    image.setLayoutParams(new LinearLayout.LayoutParams(-1, -1))
    val qrcode = QRCode.from(Parser.generate(currentProfile))
      .withSize(dp2px(250), dp2px(250)).asInstanceOf[QRCode]
    image.setImageBitmap(qrcode.bitmap())

    new AlertDialog.Builder(this)
      .setCancelable(true)
      .setNegativeButton(getString(R.string.close),
        ((dialog: DialogInterface, id: Int) => dialog.cancel()): DialogInterface.OnClickListener)
      .setView(image)
      .create()
      .show()
  }

  private def flushDnsCache() {
    val h = showProgress(R.string.flushing)
    Future {
      Utils.toggleAirplaneMode(getBaseContext)
      h.sendEmptyMessage(0)
    }
  }

  override def onActivityResult(requestCode: Int, resultCode: Int, data: Intent) {
    val scanResult = IntentIntegrator.parseActivityResult(requestCode, resultCode, data)
    if (scanResult != null) {
      Parser.parse(scanResult.getContents) match {
        case Some(profile) => addProfile(profile)
        case _ => // ignore
      }
    } else {
      resultCode match {
        case Activity.RESULT_OK =>
          prepared = true
          serviceStart()
        case _ =>
          cancelStart()
          Log.e(Shadowsocks.TAG, "Failed to start VpnService")
      }
    }
  }

  var isRoot: Option[Boolean] = None
  def isVpnEnabled: Boolean = {
    if (isRoot.isEmpty) isRoot = Some(Console.isRoot)
    !(isRoot.get && status.getBoolean(Key.isNAT, !Utils.isLollipopOrAbove))
  }

  def serviceStop() {
    if (bgService != null) bgService.stop()
  }

  def checkText(key: String): Boolean = {
    val text = settings.getString(key, "")
    !isTextEmpty(text, getString(R.string.proxy_empty))
  }

  def checkNumber(key: String, low: Boolean): Boolean = {
    val text = settings.getString(key, "")
    if (isTextEmpty(text, getString(R.string.port_empty))) return false
    try {
      val port: Int = Integer.valueOf(text)
      if (!low && port <= 1024) {
        Snackbar.make(getWindow.getDecorView.findViewById(android.R.id.content), R.string.port_alert,
          Snackbar.LENGTH_LONG).show
        return false
      }
    } catch {
      case ex: Exception =>
        Snackbar.make(getWindow.getDecorView.findViewById(android.R.id.content), R.string.port_alert,
          Snackbar.LENGTH_LONG).show
        return false
    }
    true
  }

  /** Called when connect button is clicked. */
  def serviceStart() {
    bgService.start(ConfigUtils.load(settings))

    if (isVpnEnabled) {
      changeSwitch(checked = false)
    }
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
      case ex: PackageManager.NameNotFoundException =>
        versionName = ""
    }

    new AlertDialog.Builder(this)
      .setTitle(getString(R.string.about_title).formatLocal(Locale.ENGLISH, versionName))
      .setCancelable(false)
      .setNegativeButton(getString(R.string.ok_iknow),
        ((dialog: DialogInterface, id: Int) => dialog.cancel()): DialogInterface.OnClickListener)
      .setView(web)
      .create()
      .show()
  }

  def clearDialog() {
    if (progressDialog != null) {
      progressDialog.dismiss()
      progressDialog = null
      progressTag = -1
    }
  }

  def onStateChanged(s: Int, m: String) {
    handler.post(() => if (state != s) {
      s match {
        case State.CONNECTING =>
          fab.setBackgroundTintList(greyTint);
          fab.setImageResource(R.drawable.ic_cloud_queue)
          fab.setEnabled(false)
          fabProgressCircle.show()
          setPreferenceEnabled(enabled = false)
        case State.CONNECTED =>
          fab.setBackgroundTintList(greenTint);
          if (state == State.CONNECTING) {
            fabProgressCircle.beginFinalAnimation()
          } else {
            handler.postDelayed(() => fabProgressCircle.hide(), 1000)
          }
          fab.setEnabled(true)
          changeSwitch(checked = true)
          setPreferenceEnabled(enabled = false)
        case State.STOPPED =>
          fab.setBackgroundTintList(greyTint);
          handler.postDelayed(() => fabProgressCircle.hide(), 1000)
          fab.setEnabled(true)
          changeSwitch(checked = false)
          if (m != null) Snackbar.make(getWindow.getDecorView.findViewById(android.R.id.content),
            getString(R.string.vpn_error).formatLocal(Locale.ENGLISH, m), Snackbar.LENGTH_LONG).show
          setPreferenceEnabled(enabled = true)
        case State.STOPPING =>
          fab.setBackgroundTintList(greyTint);
          fab.setImageResource(R.drawable.ic_cloud_queue)
          fab.setEnabled(false)
          fabProgressCircle.show()
          setPreferenceEnabled(enabled = false)
      }
      state = s
    })
  }

  class PreferenceBroadcastReceiver extends BroadcastReceiver {
    override def onReceive(context: Context, intent: Intent) {
      currentProfile = profileManager.save()
      menuAdapter.updateList(getMenuList, currentProfile.id)
    }
  }

}
