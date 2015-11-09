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

import android.content.pm.PackageManager
import android.content.{ClipData, ClipboardManager, Context, SharedPreferences}
import android.graphics.PixelFormat
import android.graphics.drawable.Drawable
import android.os.{Bundle, Handler}
import android.preference.PreferenceManager
import android.support.v7.app.AppCompatActivity
import android.support.v7.widget.Toolbar
import android.support.v7.widget.Toolbar.OnMenuItemClickListener
import android.view.View.OnClickListener
import android.view.ViewGroup.LayoutParams
import android.view.{MenuItem, View, ViewGroup, WindowManager}
import android.widget.AbsListView.OnScrollListener
import android.widget.CompoundButton.OnCheckedChangeListener
import android.widget._
import com.github.shadowsocks.utils.{Key, Utils}

import scala.concurrent.ExecutionContext.Implicits.global
import scala.concurrent.Future
import scala.language.implicitConversions

case class ProxiedApp(uid: Int, name: String, packageName: String, icon: Drawable, var proxied: Boolean)

class ObjectArrayTools[T <: AnyRef](a: Array[T]) {
  def binarySearch(key: T) = {
    java.util.Arrays.binarySearch(a.asInstanceOf[Array[AnyRef]], key)
  }
}

case class ListEntry(switch: Switch, text: TextView, icon: ImageView)

object AppManager {

  implicit def anyrefarray_tools[T <: AnyRef](a: Array[T]): ObjectArrayTools[T] = new ObjectArrayTools(a)

  def getProxiedApps(context: Context, proxiedAppString: String): Array[ProxiedApp] = {

    val proxiedApps = proxiedAppString.split('|').sortWith(_ < _)

    import scala.collection.JavaConversions._

    val packageManager: PackageManager = context.getPackageManager
    val appList = packageManager.getInstalledApplications(0)

    appList.filter(_.uid >= 10000).map {
      case a =>
        val uid = a.uid
        val userName = uid.toString
        val name = packageManager.getApplicationLabel(a).toString
        val packageName = a.packageName
        val proxied = proxiedApps.binarySearch(userName) >= 0
        new ProxiedApp(uid, name, packageName, null, proxied)
    }.toArray
  }
}

class AppManager extends AppCompatActivity with OnCheckedChangeListener with OnClickListener
  with OnMenuItemClickListener {

  val MSG_LOAD_START = 1
  val MSG_LOAD_FINISH = 2
  val STUB = android.R.drawable.sym_def_app_icon

  implicit def anyrefarray_tools[T <: AnyRef](a: Array[T]): ObjectArrayTools[T] = new ObjectArrayTools(a)

  var apps: Array[ProxiedApp] = _
  var appListView: ListView = _
  var loadingView: View = _
  var overlay: TextView = _
  var adapter: ListAdapter = _
  @volatile var appsLoading: Boolean = _

  def loadApps(context: Context): Array[ProxiedApp] = {
    val proxiedAppString = ShadowsocksApplication.settings.getString(Key.proxied, "")
    val proxiedApps = proxiedAppString.split('|').sortWith(_ < _)

    import scala.collection.JavaConversions._

    val packageManager: PackageManager = context.getPackageManager
    val appList = packageManager.getInstalledApplications(0)

    appList.filter(a => a.uid >= 10000
      && packageManager.getApplicationLabel(a) != null
      && packageManager.getApplicationIcon(a) != null).map {
      a =>
        val uid = a.uid
        val userName = uid.toString
        val name = packageManager.getApplicationLabel(a).toString
        val packageName = a.packageName
        val proxied = (proxiedApps binarySearch userName) >= 0
        new ProxiedApp(uid, name, packageName, a.loadIcon(packageManager), proxied)
    }.toArray
  }

  def loadApps() {
    appsLoading = true
    apps = loadApps(this).sortWith((a, b) => {
      if (a == null || b == null || a.name == null || b.name == null) {
        true
      } else if (a.proxied == b.proxied) {
        a.name < b.name
      } else if (a.proxied) {
        true
      } else {
        false
      }
    })
    adapter = new ArrayAdapter[ProxiedApp](this, R.layout.layout_apps_item, R.id.itemtext, apps) {
      override def getView(position: Int, view: View, parent: ViewGroup): View = {
        var convertView = view
        var entry: ListEntry = null
        if (convertView == null) {
          convertView = getLayoutInflater.inflate(R.layout.layout_apps_item, parent, false)
          val icon = convertView.findViewById(R.id.itemicon).asInstanceOf[ImageView]
          val switch = convertView.findViewById(R.id.itemcheck).asInstanceOf[Switch]
          val text = convertView.findViewById(R.id.itemtext).asInstanceOf[TextView]
          entry = new ListEntry(switch, text, icon)
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
        switch.setChecked(app.proxied)
        entry.text.setTag(switch)
        convertView
      }
    }
  }

  /** Called an application is check/unchecked */
  def onCheckedChanged(buttonView: CompoundButton, isChecked: Boolean) {
    val app: ProxiedApp = buttonView.getTag.asInstanceOf[ProxiedApp]
    if (app != null) {
      app.proxied = isChecked
    }
    saveAppSettings(this)
  }

  def onClick(v: View) {
    val switch = v.getTag.asInstanceOf[ListEntry].switch
    val app: ProxiedApp = switch.getTag.asInstanceOf[ProxiedApp]
    if (app != null) {
      app.proxied = !app.proxied
      switch.setChecked(app.proxied)
    }
    saveAppSettings(this)
  }

  override def onDestroy() {
    super.onDestroy()
    if (handler != null) {
      handler.removeCallbacksAndMessages(null)
      handler = null
    }
  }

  def onMenuItemClick(item: MenuItem): Boolean = {
    val clipboard = getSystemService(Context.CLIPBOARD_SERVICE).asInstanceOf[ClipboardManager]
    val prefs = PreferenceManager.getDefaultSharedPreferences(getBaseContext)
    item.getItemId match {
      case R.id.action_export =>
        val bypass = prefs.getBoolean(Key.isBypassApps, false)
        val proxiedAppString = prefs.getString(Key.proxied, "")
        val clip = ClipData.newPlainText(Key.proxied, bypass + " " + proxiedAppString)
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
                val array = proxiedAppString.split(" ")
                val bypass = array(0).toBoolean
                val apps = if (array.size > 1) array(1) else ""
                prefs.edit.putBoolean(Key.isBypassApps, bypass).apply()
                prefs.edit.putString(Key.proxied, apps).apply()
                Toast.makeText(this, R.string.action_import_msg, Toast.LENGTH_SHORT).show()
                // Restart activity
                appListView.setVisibility(View.GONE)
                loadingView.setVisibility(View.VISIBLE)
                if (appsLoading) appsLoading = false else loadAppsAsync()
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
    val toolbar = findViewById(R.id.toolbar).asInstanceOf[Toolbar]
    toolbar.setTitle(R.string.proxied_apps)
    toolbar.setNavigationIcon(R.drawable.abc_ic_ab_back_mtrl_am_alpha)
    toolbar.setNavigationOnClickListener((v: View) => {
      val intent = getParentActivityIntent
      if (intent == null) finish else navigateUpTo(intent)
    })
    toolbar.inflateMenu(R.menu.app_manager_menu)
    toolbar.setOnMenuItemClickListener(this)

    this.overlay = View.inflate(this, R.layout.overlay, null).asInstanceOf[TextView]
    getWindowManager.addView(overlay, new
        WindowManager.LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT,
          WindowManager.LayoutParams.TYPE_APPLICATION,
          WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE |
            WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE, PixelFormat.TRANSLUCENT))

    findViewById(R.id.onSwitch).asInstanceOf[Switch]
      .setOnCheckedChangeListener((button: CompoundButton, checked: Boolean) => {
        ShadowsocksApplication.settings.edit().putBoolean(Key.isProxyApps, checked).apply()
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
          val name: String = apps(firstVisibleItem).name
          if (name != null && name.length > 1) {
            overlay.setText(apps(firstVisibleItem).name.substring(0, 1))
          } else {
            overlay.setText("*")
          }
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
    loadAppsAsync()
  }

  def loadAppsAsync() {
    Future {
      while (!appsLoading) loadApps()
      appsLoading = false
      handler.post(() => {
        appListView.setAdapter(adapter)
        Utils.crossFade(AppManager.this, loadingView, appListView)
      })
    }
  }

  def saveAppSettings(context: Context) {
    if (apps == null) return
    val proxiedApps = new StringBuilder
    apps.foreach(app =>
      if (app.proxied) {
        proxiedApps ++= app.uid.toString
        proxiedApps += '|'
      })
    val edit: SharedPreferences.Editor = ShadowsocksApplication.settings.edit
    edit.putString(Key.proxied, proxiedApps.toString())
    edit.apply
  }

  var handler: Handler = null

}
