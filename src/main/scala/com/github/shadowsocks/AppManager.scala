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

import android.app.Activity
import android.app.ProgressDialog
import android.content.Context
import android.content.SharedPreferences
import android.content.pm.ApplicationInfo
import android.content.pm.PackageManager
import android.graphics.PixelFormat
import android.os.Bundle
import android.os.Handler
import android.os.Message
import android.preference.PreferenceManager
import android.view.LayoutInflater
import android.view.View
import android.view.View.OnClickListener
import android.view.ViewGroup
import android.view.ViewGroup.LayoutParams
import android.view.WindowManager
import android.widget.AbsListView
import android.widget.AbsListView.OnScrollListener
import android.widget.ArrayAdapter
import android.widget.CheckBox
import android.widget.CompoundButton
import android.widget.CompoundButton.OnCheckedChangeListener
import android.widget.ImageView
import android.widget.ListAdapter
import android.widget.ListView
import android.widget.TextView
import com.actionbarsherlock.app.SherlockActivity
import java.util.StringTokenizer
import scala.collection.mutable.ArrayBuffer

object AppManager {

  val PREFS_KEY_PROXYED = "Proxyed"

  class ObjectArrayTools[T <: AnyRef](a: Array[T]) {
    def binarySearch(key: T) = {
      java.util.Arrays.binarySearch(a.asInstanceOf[Array[AnyRef]], key)
    }
  }

  implicit def anyrefarray_tools[T <: AnyRef](a: Array[T]) = new ObjectArrayTools(a)

  def getProxiedApps(context: Context): Array[ProxiedApp] = {
    val prefs: SharedPreferences = PreferenceManager.getDefaultSharedPreferences(context)
    val proxiedAppString = prefs.getString(PREFS_KEY_PROXYED, "")
    val st: StringTokenizer = new StringTokenizer(proxiedAppString, "|")
    val proxiedApps = new Array[String](st.countTokens)
    var idx = 0
    while (st.hasMoreTokens) {
      proxiedApps(idx) = st.nextToken()
      idx += 1
    }
    proxiedApps.sortWith(_ < _)
    val pMgr: PackageManager = context.getPackageManager
    val lAppInfo = pMgr.getInstalledApplications(0)
    val itAppInfo = lAppInfo.iterator
    val result = new ArrayBuffer[ProxiedApp]
    var aInfo: ApplicationInfo = null
    while (itAppInfo.hasNext) {
      aInfo = itAppInfo.next
      if (aInfo.uid >= 10000) {
        val app: ProxiedApp = new ProxiedApp
        app.setUid(aInfo.uid)
        app.setUsername(pMgr.getNameForUid(app.getUid))
        if (aInfo.packageName != null && (aInfo.packageName == "com.github.shadowsocks")) {
          app.setProxyed(true)
        } else if (proxiedApps.binarySearch(app.getUsername) >= 0) {
          app.setProxyed(true)
        } else {
          app.setProxyed(false)
        }
        result += app
      }
    }
    result.toArray
  }

  class ListEntry {
    var box: CheckBox = null
    var text: TextView = null
    var icon: ImageView = null
  }

}

class AppManager extends SherlockActivity with OnCheckedChangeListener with OnClickListener {

  implicit def anyrefarray_tools[T <: AnyRef](a: Array[T]) = new AppManager.ObjectArrayTools(a)

  val MSG_LOAD_START = 1
  val MSG_LOAD_FINISH = 2

  def initApps(context: Context) {
    val prefs: SharedPreferences = PreferenceManager.getDefaultSharedPreferences(context)
    val proxiedAppString = prefs.getString(AppManager.PREFS_KEY_PROXYED, "")
    val st: StringTokenizer = new StringTokenizer(proxiedAppString, "|")
    var proxiedApps = new Array[String](st.countTokens)
    var idx = 0
    while (st.hasMoreTokens) {
      proxiedApps(idx) = st.nextToken()
      idx += 1
    }
    proxiedApps = proxiedApps.sortWith(_ < _)
    val pMgr: PackageManager = context.getPackageManager
    val lAppInfo = pMgr.getInstalledApplications(0)
    val itAppInfo = lAppInfo.iterator
    val result = new ArrayBuffer[ProxiedApp]
    var aInfo: ApplicationInfo = null

    while (itAppInfo.hasNext) {
      aInfo = itAppInfo.next
      aInfo match {
        case null => {}
        case a if a.uid < 10000 => {}
        case a if a.processName == null => {}
        case a if pMgr.getApplicationLabel(a) == null => {}
        case a if (pMgr.getApplicationLabel(aInfo).toString == "") => {}
        case a if pMgr.getApplicationIcon(aInfo) == null => {}
        case _ => {
          val tApp: ProxiedApp = new ProxiedApp
          tApp.setEnabled(aInfo.enabled)
          tApp.setUid(aInfo.uid)
          tApp.setUsername(pMgr.getNameForUid(tApp.getUid))
          tApp.setProcname(aInfo.processName)
          tApp.setName(pMgr.getApplicationLabel(aInfo).toString)
          tApp.setProxyed((proxiedApps binarySearch tApp.getUsername) >= 0)
          result += tApp
        }
      }
    }
    apps = result.toArray
  }

  def loadApps() {
    initApps(this)
    apps = apps.sortWith((a, b) => {
      if (a == null || b == null || a.getName == null || b.getName == null) true
      else if (a.isProxied == b.isProxied) a.getName < b.getName
      else if (a.isProxied) true
      else false
    })
    val inflater: LayoutInflater = getLayoutInflater
    adapter = new ArrayAdapter[ProxiedApp](this, R.layout.layout_apps_item, R.id.itemtext, apps) {
      override def getView(position: Int, view: View, parent: ViewGroup): View = {
        var convertView = view
        var entry: AppManager.ListEntry = null
        if (convertView == null) {
          convertView = inflater.inflate(R.layout.layout_apps_item, parent, false)
          entry = new AppManager.ListEntry
          entry.icon = convertView.findViewById(R.id.itemicon).asInstanceOf[ImageView]
          entry.box = convertView.findViewById(R.id.itemcheck).asInstanceOf[CheckBox]
          entry.text = convertView.findViewById(R.id.itemtext).asInstanceOf[TextView]
          entry.text.setOnClickListener(AppManager.this)
          entry.text.setOnClickListener(AppManager.this)
          convertView.setTag(entry)
          entry.box.setOnCheckedChangeListener(AppManager.this)
        }
        else {
          entry = convertView.getTag.asInstanceOf[AppManager.ListEntry]
        }
        val app: ProxiedApp = apps(position)
        entry.icon.setTag(app.getUid)
        imageLoader.DisplayImage(app.getUid, convertView.getContext.asInstanceOf[Activity], entry.icon)
        entry.text.setText(app.getName)
        val box: CheckBox = entry.box
        box.setTag(app)
        box.setChecked(app.isProxied)
        entry.text.setTag(box)
        convertView
      }
    }
    appsLoaded = true
  }

  /** Called an application is check/unchecked */
  def onCheckedChanged(buttonView: CompoundButton, isChecked: Boolean) {
    val app: ProxiedApp = buttonView.getTag.asInstanceOf[ProxiedApp]
    if (app != null) {
      app.setProxyed(isChecked)
    }
    saveAppSettings(this)
  }

  def onClick(v: View) {
    val cbox: CheckBox = v.getTag.asInstanceOf[CheckBox]
    val app: ProxiedApp = cbox.getTag.asInstanceOf[ProxiedApp]
    if (app != null) {
      app.setProxyed(!app.isProxied)
      cbox.setChecked(app.isProxied)
    }
    saveAppSettings(this)
  }

  protected override def onCreate(savedInstanceState: Bundle) {
    super.onCreate(savedInstanceState)
    this.setContentView(R.layout.layout_apps)
    this.imageLoader = ImageLoader.getImageLoader(this)
    this.overlay = View.inflate(this, R.layout.overlay, null).asInstanceOf[TextView]
    getWindowManager.addView(overlay, new WindowManager.LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT, WindowManager.LayoutParams.TYPE_APPLICATION, WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE | WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE, PixelFormat.TRANSLUCENT))
  }

  protected override def onResume() {
    super.onResume()
    new Thread {
      override def run() {
        handler.sendEmptyMessage(MSG_LOAD_START)
        appListView = findViewById(R.id.applistview).asInstanceOf[ListView]
        if (!appsLoaded) loadApps()
        handler.sendEmptyMessage(MSG_LOAD_FINISH)
      }
    }.start()
  }

  protected override def onStop() {
    super.onStop()
  }

  def saveAppSettings(context: Context) {
    if (apps == null) return
    val prefs: SharedPreferences = PreferenceManager.getDefaultSharedPreferences(this)
    val proxiedApps = new StringBuilder
    apps.foreach(app => {
      if (app.isProxied) {
        proxiedApps ++= app.getUsername
        proxiedApps += '|'
      }
    }
    )
    val edit: SharedPreferences.Editor = prefs.edit
    edit.putString(AppManager.PREFS_KEY_PROXYED, proxiedApps.toString())
    edit.commit
  }

  val handler: Handler = new Handler {
    override def handleMessage(msg: Message) {
      msg.what match {
        case MSG_LOAD_START =>
          progressDialog = ProgressDialog.show(AppManager.this, "", getString(R.string.loading), true, true)
        case MSG_LOAD_FINISH =>
          appListView.setAdapter(adapter)
          appListView.setOnScrollListener(new AbsListView.OnScrollListener {
            def onScroll(view: AbsListView, firstVisibleItem: Int, visibleItemCount: Int, totalItemCount: Int) {
              if (visible) {
                val name: String = apps(firstVisibleItem).getName
                if (name != null && name.length > 1) {
                  overlay.setText(apps(firstVisibleItem).getName.substring(0, 1))
                }
                else {
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

            var visible = false
          })
          if (progressDialog != null) {
            progressDialog.dismiss()
            progressDialog = null
          }
      }
      super.handleMessage(msg)
    }
  }
  var apps: Array[ProxiedApp] = null
  var appListView: ListView = null
  var overlay: TextView = null
  var progressDialog: ProgressDialog = null
  var adapter: ListAdapter = null
  var imageLoader: ImageLoader = null
  var appsLoaded: Boolean = false
}