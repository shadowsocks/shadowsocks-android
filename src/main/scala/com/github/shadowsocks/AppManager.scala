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

import java.util.concurrent.atomic.AtomicBoolean

import android.Manifest.permission
import android.content._
import android.content.pm.PackageManager
import android.graphics.PixelFormat
import android.graphics.drawable.Drawable
import android.os.{Bundle, Handler}
import android.preference.PreferenceManager
import android.support.v7.app.AppCompatActivity
import android.support.v7.widget.Toolbar
import android.support.v7.widget.Toolbar.OnMenuItemClickListener
import android.view.View.OnClickListener
import android.view.ViewGroup.LayoutParams
import android.view._
import android.widget.AbsListView.OnScrollListener
import android.widget.CompoundButton.OnCheckedChangeListener
import android.widget._
import com.github.shadowsocks.utils.{Key, Utils}

import scala.collection.JavaConversions._
import scala.collection.mutable
import scala.language.implicitConversions

object AppManager {
  case class ProxiedApp(name: String, packageName: String, icon: Drawable)
  private case class ListEntry(switch: Switch, text: TextView, icon: ImageView)

  private var instance: AppManager = _

  private var receiverRegistered: Boolean = _
  private var cachedApps: Array[ProxiedApp] = _
  private def getApps(pm: PackageManager) = {
    if (!receiverRegistered) {
      val filter = new IntentFilter(Intent.ACTION_PACKAGE_ADDED)
      filter.addAction(Intent.ACTION_PACKAGE_REMOVED)
      filter.addDataScheme("package")
      ShadowsocksApplication.instance.registerReceiver((context: Context, intent: Intent) =>
        if (intent.getAction != Intent.ACTION_PACKAGE_REMOVED ||
          !intent.getBooleanExtra(Intent.EXTRA_REPLACING, false)) {
          synchronized(cachedApps = null)
          val instance = AppManager.instance
          if (instance != null) instance.reloadApps()
        }, filter)
      receiverRegistered = true
    }
    synchronized {
      if (cachedApps == null) cachedApps = pm.getInstalledPackages(PackageManager.GET_PERMISSIONS)
        .filter(p => p.requestedPermissions != null && p.requestedPermissions.contains(permission.INTERNET))
        .map(p => new ProxiedApp(pm.getApplicationLabel(p.applicationInfo).toString, p.packageName,
          p.applicationInfo.loadIcon(pm))).toArray
      cachedApps
    }
  }
}

class AppManager extends AppCompatActivity with OnCheckedChangeListener with OnClickListener
  with OnMenuItemClickListener {
  import AppManager._

  private var apps: Array[ProxiedApp] = _
  private var proxiedApps: mutable.HashSet[String] = _
  private var toolbar: Toolbar = _
  private var appListView: ListView = _
  private var loadingView: View = _
  private var overlay: TextView = _
  private var adapter: ListAdapter = _
  private val appsLoading = new AtomicBoolean

  private def setProxied(pn: String, proxied: Boolean) = if (proxied) proxiedApps.add(pn) else proxiedApps.remove(pn)

  /** Called an application is check/unchecked */
  def onCheckedChanged(buttonView: CompoundButton, isChecked: Boolean) {
    val app: ProxiedApp = buttonView.getTag.asInstanceOf[ProxiedApp]
    if (app != null) setProxied(app.packageName, isChecked)
    saveAppSettings(this)
  }

  def onClick(v: View) {
    val switch = v.getTag.asInstanceOf[ListEntry].switch
    val app: ProxiedApp = switch.getTag.asInstanceOf[ProxiedApp]
    if (app != null) {
      val proxied = !proxiedApps.contains(app.packageName)
      setProxied(app.packageName, proxied)
      switch.setChecked(proxied)
    }
    saveAppSettings(this)
  }

  override def onDestroy() {
    instance = null
    super.onDestroy()
    if (handler != null) {
      handler.removeCallbacksAndMessages(null)
      handler = null
    }
    if (overlay != null) {
      getWindowManager.removeViewImmediate(overlay)
      overlay = null
    }
  }

  def onMenuItemClick(item: MenuItem): Boolean = {
    val clipboard = getSystemService(Context.CLIPBOARD_SERVICE).asInstanceOf[ClipboardManager]
    val prefs = PreferenceManager.getDefaultSharedPreferences(getBaseContext)
    item.getItemId match {
      case R.id.action_export =>
        val bypass = prefs.getBoolean(Key.isBypassApps, false)
        val proxiedAppString = prefs.getString(Key.proxied, "")
        val clip = ClipData.newPlainText(Key.proxied, bypass + "\n" + proxiedAppString)
        clipboard.setPrimaryClip(clip)
        Toast.makeText(this, R.string.action_export_msg, Toast.LENGTH_SHORT).show()
        return true
      case R.id.action_import =>
        if (clipboard.hasPrimaryClip) {
          val clipdata = clipboard.getPrimaryClip
          val label = clipdata.getDescription.getLabel
          if (label == Key.proxied) {
            val proxiedAppSequence = clipdata.getItemAt(0).getText
            if (proxiedAppSequence != null) {
              val proxiedAppString = proxiedAppSequence.toString
              if (!proxiedAppString.isEmpty) {
                val editor = prefs.edit
                val i = proxiedAppString.indexOf('\n')
                if (i < 0)
                  editor.putBoolean(Key.isBypassApps, proxiedAppString.toBoolean).putString(Key.proxied, "").apply()
                else editor.putBoolean(Key.isBypassApps, proxiedAppString.substring(0, i).toBoolean)
                  .putString(Key.proxied, proxiedAppString.substring(i + 1)).apply()
                Toast.makeText(this, R.string.action_import_msg, Toast.LENGTH_SHORT).show()
                // Restart activity
                appListView.setVisibility(View.GONE)
                loadingView.setVisibility(View.VISIBLE)
                reloadApps()
                return true
              }
            }
          }
        }
        Toast.makeText(this, R.string.action_import_err, Toast.LENGTH_SHORT).show()
        return false
    }
    false
  }

  protected override def onCreate(savedInstanceState: Bundle) {
    super.onCreate(savedInstanceState)

    handler = new Handler()

    this.setContentView(R.layout.layout_apps)
    toolbar = findViewById(R.id.toolbar).asInstanceOf[Toolbar]
    toolbar.setTitle(R.string.proxied_apps)
    toolbar.setNavigationIcon(R.drawable.abc_ic_ab_back_mtrl_am_alpha)
    toolbar.setNavigationOnClickListener((v: View) => {
      val intent = getParentActivityIntent
      if (intent == null) finish else navigateUpTo(intent)
    })
    toolbar.inflateMenu(R.menu.app_manager_menu)
    toolbar.setOnMenuItemClickListener(this)

    overlay = View.inflate(this, R.layout.overlay, null).asInstanceOf[TextView]
    getWindowManager.addView(overlay, new
        WindowManager.LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT,
          WindowManager.LayoutParams.TYPE_APPLICATION,
          WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE |
            WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE, PixelFormat.TRANSLUCENT))

    ShadowsocksApplication.settings.edit().putBoolean(Key.isProxyApps, true).commit()
    findViewById(R.id.onSwitch).asInstanceOf[Switch]
      .setOnCheckedChangeListener((button: CompoundButton, checked: Boolean) => {
        ShadowsocksApplication.settings.edit().putBoolean(Key.isProxyApps, checked).commit()
        finish()
      })

    val bypassSwitch = findViewById(R.id.bypassSwitch).asInstanceOf[Switch]
    bypassSwitch.setOnCheckedChangeListener((button: CompoundButton, checked: Boolean) =>
      ShadowsocksApplication.settings.edit().putBoolean(Key.isBypassApps, checked).apply())
    bypassSwitch.setChecked(ShadowsocksApplication.settings.getBoolean(Key.isBypassApps, false))

    loadingView = findViewById(R.id.loading)
    appListView = findViewById(R.id.applistview).asInstanceOf[ListView]
    appListView.setOnScrollListener(new AbsListView.OnScrollListener {
      var visible = false
      def onScroll(view: AbsListView, firstVisibleItem: Int, visibleItemCount: Int,
                   totalItemCount: Int) {
        if (visible) {
          val name = apps(firstVisibleItem).name
          overlay.setText(if (name != null && name.length > 1) name(0).toUpper.toString else "*")
          overlay.setVisibility(View.VISIBLE)
        }
      }
      def onScrollStateChanged(view: AbsListView, scrollState: Int) {
        visible = true
        if (scrollState == OnScrollListener.SCROLL_STATE_IDLE) {
          overlay.setVisibility(View.INVISIBLE)
        }
      }
    })
    instance = this
    loadAppsAsync()
  }

  def reloadApps() = if (!appsLoading.compareAndSet(true, false)) loadAppsAsync()
  def loadAppsAsync() {
    if (!appsLoading.compareAndSet(false, true)) return
    ThrowableFuture {
      do {
        proxiedApps = ShadowsocksApplication.settings.getString(Key.proxied, "").split('\n').to[mutable.HashSet]
        appsLoading.set(true)
        apps = getApps(getPackageManager).sortWith((a, b) => {
          val aProxied = proxiedApps.contains(a.packageName)
          if (aProxied ^ proxiedApps.contains(b.packageName)) aProxied else a.name.compareToIgnoreCase(b.name) < 0
        })
        adapter = new ArrayAdapter[ProxiedApp](this, R.layout.layout_apps_item, R.id.itemtext, apps) {
          override def getView(position: Int, view: View, parent: ViewGroup): View = {
            var convertView = view
            var entry: ListEntry = null
            if (convertView == null) {
              convertView = getLayoutInflater.inflate(R.layout.layout_apps_item, parent, false)
              entry = new ListEntry(convertView.findViewById(R.id.itemcheck).asInstanceOf[Switch],
                convertView.findViewById(R.id.itemtext).asInstanceOf[TextView],
                convertView.findViewById(R.id.itemicon).asInstanceOf[ImageView])
              convertView.setOnClickListener(AppManager.this)
              convertView.setTag(entry)
              entry.switch.setOnCheckedChangeListener(AppManager.this)
            } else {
              entry = convertView.getTag.asInstanceOf[ListEntry]
            }

            val app: ProxiedApp = apps(position)

            entry.text.setText(app.name)
            entry.icon.setImageDrawable(app.icon)
            val switch = entry.switch
            switch.setTag(app)
            switch.setChecked(proxiedApps.contains(app.packageName))
            entry.text.setTag(switch)
            convertView
          }
        }
      } while (!appsLoading.compareAndSet(true, false))
      handler.post(() => {
        appListView.setAdapter(adapter)
        Utils.crossFade(AppManager.this, loadingView, appListView)
      })
    }
  }

  def saveAppSettings(context: Context) {
    if (!appsLoading.get) ShadowsocksApplication.settings.edit.putString(Key.proxied, proxiedApps.mkString("\n")).apply
  }

  var handler: Handler = null

  override def onKeyUp(keyCode: Int, event: KeyEvent) = keyCode match {
    case KeyEvent.KEYCODE_MENU =>
      if (toolbar.isOverflowMenuShowing) toolbar.hideOverflowMenu else toolbar.showOverflowMenu
    case _ => super.onKeyUp(keyCode, event)
  }
}
