/* Shadowsocks - A shadowsocks client for Android
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
import android.content.Context
import android.content.DialogInterface
import android.content.Intent
import android.content.SharedPreferences
import android.content.SharedPreferences.OnSharedPreferenceChangeListener
import android.content.res.AssetManager
import android.graphics.Typeface
import android.os.{Build, Bundle, Handler, Message}
import android.preference.Preference
import android.preference.PreferenceManager
import android.util.Log
import android.view.KeyEvent
import android.widget.CompoundButton
import android.widget.RelativeLayout
import android.widget.TextView
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
import android.content.pm.PackageManager
import android.net.VpnService

object Shadowsocks {
  val PREFS_NAME = "Shadowsocks"
  val PROXY_PREFS = Array("proxy", "remotePort", "port", "sitekey", "encMethod")
  val FEATRUE_PREFS = Array("isHTTPProxy", "isDNSProxy", "isGFWList", "isGlobalProxy", "isBypassApps", "proxyedApps", "isAutoConnect")
  val TAG = "Shadowsocks"
  val REQUEST_CONNECT = 1

  class ProxyFragment extends UnifiedPreferenceFragment with OnSharedPreferenceChangeListener {
    private def setPreferenceEnabled() {
      val settings: SharedPreferences = PreferenceManager.getDefaultSharedPreferences(getActivity)
      val enabled: Boolean = !settings.getBoolean("isRunning", false) && !settings.getBoolean("isConnecting", false)
      for (name <- PROXY_PREFS) {
        val pref: Preference = findPreference(name)
        if (pref != null) {
          pref.setEnabled(enabled)
        }
      }
    }

    override def onResume() {
      super.onResume()
      setPreferenceEnabled()
      getPreferenceScreen.getSharedPreferences.registerOnSharedPreferenceChangeListener(this)
    }

    override def onPause() {
      super.onPause()
      getPreferenceScreen.getSharedPreferences.unregisterOnSharedPreferenceChangeListener(this)
    }

    def onSharedPreferenceChanged(sharedPreferences: SharedPreferences, key: String) {
      if ((key == "isRunning") || (key == "isGlobalProxy")) {
        setPreferenceEnabled()
      }
    }
  }

  class FeatureFragment extends UnifiedPreferenceFragment with OnSharedPreferenceChangeListener {
    private def setPreferenceEnabled() {
      val settings: SharedPreferences = PreferenceManager.getDefaultSharedPreferences(getActivity)
      val enabled: Boolean = !settings.getBoolean("isRunning", false) && !settings.getBoolean("isConnecting", false)
      for (name <- FEATRUE_PREFS) {
        val pref: Preference = findPreference(name)
        if (pref != null) {
          if ((name == "isBypassApps") || (name == "proxyedApps")) {
            val isGlobalProxy: Boolean = settings.getBoolean("isGlobalProxy", false)
            pref.setEnabled(enabled && !isGlobalProxy)
          }
          else {
            pref.setEnabled(enabled)
          }
        }
      }
    }

    override def onResume() {
      super.onResume()
      setPreferenceEnabled()
      getPreferenceScreen.getSharedPreferences.registerOnSharedPreferenceChangeListener(this)
    }

    override def onPause() {
      super.onPause()
      getPreferenceScreen.getSharedPreferences.unregisterOnSharedPreferenceChangeListener(this)
    }

    def onSharedPreferenceChanged(sharedPreferences: SharedPreferences, key: String) {
      if ((key == "isRunning") || (key == "isGlobalProxy")) {
        setPreferenceEnabled()
      }
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
        }
        catch {
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

class Shadowsocks extends UnifiedSherlockPreferenceActivity with CompoundButton.OnCheckedChangeListener with OnSharedPreferenceChangeListener {

  private var progressDialog: ProgressDialog = null
  private val MSG_CRASH_RECOVER: Int = 1
  private val MSG_INITIAL_FINISH: Int = 2

  private def copyAssets(path: String) {
    val assetManager: AssetManager = getAssets
    var files: Array[String] = null
    try {
      files = assetManager.list(path)
    }
    catch {
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
          }
          else {
            in = assetManager.open(file)
          }
          out = new FileOutputStream("/data/data/com.github.shadowsocks/" + file)
          copyFile(in, out)
          in.close()
          in = null
          out.flush()
          out.close()
          out = null
        }
        catch {
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
    while ((({
      read = in.read(buffer)
      read
    })) != -1) {
      out.write(buffer, 0, read)
    }
  }

  private def crash_recovery() {
    val sb = new StringBuilder
    sb.append(Utils.getIptables).append(" -t nat -F OUTPUT").append("\n")
    sb.append("kill -9 `cat /data/data/com.github.shadowsocks/pdnsd.pid`").append("\n")
    sb.append("kill -9 `cat /data/data/com.github.shadowsocks/redsocks.pid`").append("\n")
    sb.append("kill -9 `cat /data/data/com.github.shadowsocks/shadowsocks.pid`").append("\n")
    sb.append("kill -9 `cat /data/data/com.github.shadowsocks/polipo.pid`").append("\n")
    sb.append("kill -9 `cat /data/data/com.github.shadowsocks/tun2socks.pid`").append("\n")
    sb.append("killall -9 pdnsd").append("\n")
    sb.append("killall -9 redsocks").append("\n")
    sb.append("killall -9 shadowsocks").append("\n")
    sb.append("killall -9 polipo").append("\n")
    sb.append("killall -9 tun2socks").append("\n")
    Utils.runRootCommand(sb.toString())
  }

  private def isTextEmpty(s: String, msg: String): Boolean = {
    if (s == null || s.length <= 0) {
      showAToast(msg)
      return true
    }
    false
  }

  def onCheckedChanged(compoundButton: CompoundButton, checked: Boolean) {
    if (compoundButton eq switchButton) {
      checked match {
        case true => {
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
        case false => {
          serviceStop()
        }
      }
    }
  }

  /** Called when the activity is first created. */
  override def onCreate(savedInstanceState: Bundle) {
    setHeaderRes(R.xml.shadowsocks_headers)
    super.onCreate(savedInstanceState)
    val switchLayout: RelativeLayout = getLayoutInflater.inflate(R.layout.layout_switch, null).asInstanceOf[RelativeLayout]
    getSupportActionBar.setCustomView(switchLayout)
    getSupportActionBar.setDisplayShowTitleEnabled(false)
    getSupportActionBar.setDisplayShowCustomEnabled(true)
    getSupportActionBar.setDisplayShowHomeEnabled(false)
    switchButton = switchLayout.findViewById(R.id.switchButton).asInstanceOf[Switch]
    val title: TextView = switchLayout.findViewById(R.id.title).asInstanceOf[TextView]
    val tf: Typeface = Typefaces.get(this, "fonts/Iceland.ttf")
    if (tf != null) title.setTypeface(tf)
    title.setText(R.string.app_name)
    val settings: SharedPreferences = PreferenceManager.getDefaultSharedPreferences(Shadowsocks.this)
    val init: Boolean = !settings.getBoolean("isRunning", false) && !settings.getBoolean("isConnecting", false)
    if (init) {
      if (progressDialog == null) {
        progressDialog = ProgressDialog.show(this, "", getString(R.string.initializing), true, true)
      }
      new Thread {
        override def run() {
          Utils.getRoot
          var versionName: String = null
          try {
            versionName = getPackageManager.getPackageInfo(getPackageName, 0).versionName
          }
          catch {
            case e: PackageManager.NameNotFoundException => {
              versionName = "NONE"
            }
          }
          if (!settings.getBoolean(versionName, false)) {
            val edit: SharedPreferences.Editor = settings.edit
            edit.putBoolean(versionName, true)
            edit.commit
            reset()
          }
          handler.sendEmptyMessage(MSG_INITIAL_FINISH)
        }
      }.start()
    }
  }

  override def onCreateOptionsMenu(menu: Menu): Boolean = {
    menu.add(0, 0, 0, R.string.recovery).setIcon(android.R.drawable.ic_menu_revert).setShowAsAction(MenuItem.SHOW_AS_ACTION_WITH_TEXT)
    menu.add(0, 1, 1, R.string.about).setIcon(android.R.drawable.ic_menu_info_details).setShowAsAction(MenuItem.SHOW_AS_ACTION_WITH_TEXT)
    true
  }

  override def onKeyDown(keyCode: Int, event: KeyEvent): Boolean = {
    if (keyCode == KeyEvent.KEYCODE_BACK && event.getRepeatCount == 0) {
      try {
        finish()
      }
      catch {
        case ignore: Exception => {
        }
      }
      return true
    }
    super.onKeyDown(keyCode, event)
  }

  override def onOptionsItemSelected(item: MenuItem): Boolean = {
    item.getItemId match {
      case 0 =>
        recovery()
      case 1 =>
        var versionName = ""
        try {
          versionName = getPackageManager.getPackageInfo(getPackageName, 0).versionName
        }
        catch {
          case ex: PackageManager.NameNotFoundException => {
            versionName = ""
          }
        }
        showAToast(getString(R.string.about) + " (" + versionName + ")\n\n" + getString(R.string.copy_rights))
    }
    super.onOptionsItemSelected(item)
  }

  protected override def onPause() {
    super.onPause()
    PreferenceManager.getDefaultSharedPreferences(this).unregisterOnSharedPreferenceChangeListener(this)
  }

  protected override def onResume() {
    super.onResume()
    val settings: SharedPreferences = PreferenceManager.getDefaultSharedPreferences(this)
    if (isServiceStarted) {
      switchButton.setChecked(true)
      if (ShadowVpnService.isServiceStarted) {
        val style = new Style.Builder()
          .setBackgroundColorValue(Style.holoBlueLight)
          .setDuration(Style.DURATION_INFINITE)
          .build()
        switchButton.setEnabled(false)
        Crouton.makeText(Shadowsocks.this, R.string.vpn_status, style).show()
      }
    }
    else {
      if (settings.getBoolean("isRunning", false)) {
        new Thread {
          override def run() {
            crash_recovery()
            handler.sendEmptyMessage(MSG_CRASH_RECOVER)
          }
        }.start()
      }
      switchButton.setChecked(false)
    }
    setPreferenceEnabled()
    switchButton.setOnCheckedChangeListener(this)
    PreferenceManager.getDefaultSharedPreferences(this).registerOnSharedPreferenceChangeListener(this)
  }

  private def setPreferenceEnabled() {
    val settings: SharedPreferences = PreferenceManager.getDefaultSharedPreferences(Shadowsocks.this)
    val enabled: Boolean = !settings.getBoolean("isRunning", false) && !settings.getBoolean("isConnecting", false)
    for (name <- Shadowsocks.PROXY_PREFS) {
      val pref: Preference = findPreference(name)
      if (pref != null) {
        pref.setEnabled(enabled)
      }
    }
    for (name <- Shadowsocks.FEATRUE_PREFS) {
      val pref: Preference = findPreference(name)
      if (pref != null) {
        if ((name == "isBypassApps") || (name == "proxyedApps")) {
          val isGlobalProxy: Boolean = settings.getBoolean("isGlobalProxy", false)
          pref.setEnabled(enabled && !isGlobalProxy)
        }
        else {
          pref.setEnabled(enabled)
        }
      }
    }
  }

  def onSharedPreferenceChanged(sharedPreferences: SharedPreferences, key: String) {
    val settings: SharedPreferences = PreferenceManager.getDefaultSharedPreferences(this)
    if (key == "isRunning") {
      if (settings.getBoolean("isRunning", false)) {
        if (!switchButton.isChecked) switchButton.setChecked(true)
      }
      else {
        if (switchButton.isChecked) switchButton.setChecked(false)
      }
    }
    if (key == "isConnecting") {
      if (settings.getBoolean("isConnecting", false)) {
        Log.d(Shadowsocks.TAG, "Connecting start")
        if (progressDialog == null) {
          progressDialog = ProgressDialog.show(this, "", getString(R.string.connecting), true, true)
        }
      }
      else {
        Log.d(Shadowsocks.TAG, "Connecting finish")
        if (progressDialog != null) {
          progressDialog.dismiss()
          progressDialog = null
        }
      }
    }
    if ((key == "isRunning") || (key == "isGlobalProxy")) {
      setPreferenceEnabled()
    }
  }

  override def onStart() {
    super.onStart()
    EasyTracker.getInstance.activityStart(this)
  }

  override def onStop() {
    super.onStop()
    EasyTracker.getInstance.activityStop(this)
    if (progressDialog != null) {
      progressDialog.dismiss()
      progressDialog = null
    }
  }

  override def onDestroy() {
    super.onDestroy()
    Crouton.cancelAllCroutons()
  }

  def reset() {
    crash_recovery()
    copyAssets("")
    copyAssets(Utils.getABI)
    Utils.runCommand("chmod 755 /data/data/com.github.shadowsocks/iptables\n"
      + "chmod 755 /data/data/com.github.shadowsocks/redsocks\n"
      + "chmod 755 /data/data/com.github.shadowsocks/pdnsd\n"
      + "chmod 755 /data/data/com.github.shadowsocks/shadowsocks\n"
      + "chmod 755 /data/data/com.github.shadowsocks/polipo\n"
      + "chmod 755 /data/data/com.github.shadowsocks/tun2socks\n")
  }

  private def recovery() {
    if (progressDialog == null) {
      progressDialog = ProgressDialog.show(this, "", getString(R.string.recovering), true, true)
    }
    val h: Handler = new Handler {
      override def handleMessage(msg: Message) {
        if (progressDialog != null) {
          progressDialog.dismiss()
          progressDialog = null
        }
      }
    }
    serviceStop()
    new Thread {
      override def run() {
        reset()
        h.sendEmptyMessage(0)
      }
    }.start()
  }

  override def onActivityResult(requestCode: Int, resultCode: Int, data: Intent) {
    resultCode match {
      case Activity.RESULT_OK => {
        if (!serviceStart) {
          switchButton.setChecked(false)
        }
      }
      case _ => {
        Log.e(Shadowsocks.TAG, "Failed to start VpnService")
      }
    }
  }

  def isVpnEnabled: Boolean = {
    Build.VERSION.SDK_INT >= Build.VERSION_CODES.ICE_CREAM_SANDWICH && !Utils.getRoot
  }

  def isServiceStarted: Boolean = {
    ShadowsocksService.isServiceStarted || ShadowVpnService.isServiceStarted
  }

  def serviceStop() {
    if (ShadowVpnService.isServiceStarted) {
      if (ShadowVpnService.sConn != null) {
        ShadowVpnService.sConn.close()
        ShadowVpnService.sConn = null
      }
      stopService(new Intent(this, classOf[ShadowVpnService]))
    }
    if (ShadowsocksService.isServiceStarted) {
      stopService(new Intent(this, classOf[ShadowsocksService]))
    }
  }

  /** Called when connect button is clicked. */
  def serviceStart: Boolean = {

    val settings: SharedPreferences = PreferenceManager.getDefaultSharedPreferences(this)
    val proxy = settings.getString("proxy", "")
    if (isTextEmpty(proxy, getString(R.string.proxy_empty))) return false
    val portText = settings.getString("port", "")
    if (isTextEmpty(portText, getString(R.string.port_empty))) return false
    try {
      val port: Int = Integer.valueOf(portText)
      if (port <= 1024) {
        this.showAToast(getString(R.string.port_alert))
        return false
      }
    }
    catch {
      case ex: Exception => {
        this.showAToast(getString(R.string.port_alert))
        return false
      }
    }

    if (isVpnEnabled) {
      if (ShadowVpnService.isServiceStarted) return false
      val it: Intent = new Intent(this, classOf[ShadowVpnService])
      startService(it)
      val style = new Style.Builder()
        .setBackgroundColorValue(Style.holoBlueLight)
        .setDuration(Style.DURATION_INFINITE)
        .build()
      Crouton.makeText(Shadowsocks.this, R.string.vpn_status, style).show()
      switchButton.setEnabled(false)
    } else {
      if (ShadowsocksService.isServiceStarted) return false
      val it: Intent = new Intent(this, classOf[ShadowsocksService])
      startService(it)
    }

    true
  }

  private def showAToast(msg: String) {
    val builder: AlertDialog.Builder = new AlertDialog.Builder(this)
    builder.setMessage(msg).setCancelable(false).setNegativeButton(getString(R.string.ok_iknow), new DialogInterface.OnClickListener {
      def onClick(dialog: DialogInterface, id: Int) {
        dialog.cancel()
      }
    })
    val alert: AlertDialog = builder.create
    alert.show()
  }

  private[shadowsocks] final val handler: Handler = new Handler {
    override def handleMessage(msg: Message) {
      val settings: SharedPreferences = PreferenceManager.getDefaultSharedPreferences(Shadowsocks.this)
      val ed: SharedPreferences.Editor = settings.edit
      msg.what match {
        case MSG_CRASH_RECOVER =>
          Crouton.makeText(Shadowsocks.this, R.string.crash_alert, Style.ALERT).show()
          ed.putBoolean("isRunning", false)
        case MSG_INITIAL_FINISH =>
          if (progressDialog != null) {
            progressDialog.dismiss()
            progressDialog = null
          }
      }
      ed.commit
      super.handleMessage(msg)
    }
  }
  private var switchButton: Switch = null
}