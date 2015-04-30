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

import android.app.{Activity, ProgressDialog}
import android.content.ClipboardManager
import android.content.ClipData
import android.content.Context
import android.content.SharedPreferences
import android.content.pm.PackageManager
import android.graphics.{Bitmap, PixelFormat}
import android.os.Bundle
import android.os.Handler
import android.preference.PreferenceManager
import android.view.Menu
import android.view.MenuItem
import android.view.View
import android.view.View.OnClickListener
import android.view.ViewGroup
import android.view.ViewGroup.LayoutParams
import android.view.WindowManager
import android.widget._
import android.widget.AbsListView.OnScrollListener
import android.widget.CompoundButton.OnCheckedChangeListener
import com.nostra13.universalimageloader.core.download.BaseImageDownloader
import java.io.{ByteArrayOutputStream, ByteArrayInputStream, InputStream}
import com.nostra13.universalimageloader.core.{DisplayImageOptions, ImageLoader, ImageLoaderConfiguration}
import com.nostra13.universalimageloader.core.display.FadeInBitmapDisplayer
import com.github.shadowsocks.utils.{Utils, Scheme, Key}

import scala.concurrent.ops._
import scala.language.implicitConversions

case class ProxiedApp(uid: Int, name: String, packageName: String, var proxied: Boolean)

class ObjectArrayTools[T <: AnyRef](a: Array[T]) {
  def binarySearch(key: T) = {
    java.util.Arrays.binarySearch(a.asInstanceOf[Array[AnyRef]], key)
  }
}

case class ListEntry(box: CheckBox, text: TextView, icon: ImageView)

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
        new ProxiedApp(uid, name, packageName, proxied)
    }.toArray
  }
}

class AppManager extends Activity with OnCheckedChangeListener with OnClickListener {

  val MSG_LOAD_START = 1
  val MSG_LOAD_FINISH = 2
  val STUB = android.R.drawable.sym_def_app_icon

  implicit def anyrefarray_tools[T <: AnyRef](a: Array[T]): ObjectArrayTools[T] = new ObjectArrayTools(a)

  var apps: Array[ProxiedApp] = null
  var appListView: ListView = null
  var overlay: TextView = null
  var progressDialog: ProgressDialog = null
  var adapter: ListAdapter = null
  var appsLoaded: Boolean = false

  def loadApps(context: Context): Array[ProxiedApp] = {
    val prefs = PreferenceManager.getDefaultSharedPreferences(context)
    val proxiedAppString = prefs.getString(Key.proxied, "")
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
        new ProxiedApp(uid, name, packageName, proxied)
    }.toArray
  }

  def loadApps() {
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
          val box = convertView.findViewById(R.id.itemcheck).asInstanceOf[CheckBox]
          val text = convertView.findViewById(R.id.itemtext).asInstanceOf[TextView]
          entry = new ListEntry(box, text, icon)
          entry.text.setOnClickListener(AppManager.this)
          entry.text.setOnClickListener(AppManager.this)
          convertView.setTag(entry)
          entry.box.setOnCheckedChangeListener(AppManager.this)
        } else {
          entry = convertView.getTag.asInstanceOf[ListEntry]
        }

        val app: ProxiedApp = apps(position)
        val options =
          new DisplayImageOptions.Builder()
            .showStubImage(STUB)
            .showImageForEmptyUri(STUB)
            .showImageOnFail(STUB)
            .resetViewBeforeLoading()
            .cacheInMemory()
            .cacheOnDisc()
            .displayer(new FadeInBitmapDisplayer(300))
            .build()
        ImageLoader.getInstance().displayImage(Scheme.APP + app.packageName, entry.icon, options)

        entry.text.setText(app.name)
        val box: CheckBox = entry.box
        box.setTag(app)
        box.setChecked(app.proxied)
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
      app.proxied = isChecked
    }
    saveAppSettings(this)
  }

  def onClick(v: View) {
    val cbox: CheckBox = v.getTag.asInstanceOf[CheckBox]
    val app: ProxiedApp = cbox.getTag.asInstanceOf[ProxiedApp]
    if (app != null) {
      app.proxied = !app.proxied
      cbox.setChecked(app.proxied)
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

  protected override def onOptionsItemSelected(item: MenuItem): Boolean = {
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
                prefs.edit.putBoolean(Key.isBypassApps, bypass).commit()
                prefs.edit.putString(Key.proxied, apps).commit()
                Toast.makeText(this, R.string.action_import_msg, Toast.LENGTH_SHORT).show()
                // Restart activity
                val intent = getIntent
                finish()
                startActivity(intent)
                return true
              }
            }
          }
        }
        Toast.makeText(this, R.string.action_import_err, Toast.LENGTH_SHORT).show()
        return false
      case android.R.id.home =>
        navigateUpTo(getParentActivityIntent)
        return true
    }
    super.onOptionsItemSelected(item)
  }

  protected override def onCreateOptionsMenu(menu: Menu): Boolean = {
    val inflater = getMenuInflater()
    inflater.inflate(R.menu.app_manager_menu, menu)
    return super.onCreateOptionsMenu(menu)
  }

  protected override def onCreate(savedInstanceState: Bundle) {
    super.onCreate(savedInstanceState)

    handler = new Handler()

    val actionBar = getActionBar()
    actionBar.setTitle(R.string.proxied_help)
    actionBar.setDisplayHomeAsUpEnabled(true)
    actionBar.setDisplayShowHomeEnabled(false)
    this.setContentView(R.layout.layout_apps)
    this.overlay = View.inflate(this, R.layout.overlay, null).asInstanceOf[TextView]
    getWindowManager.addView(overlay, new
        WindowManager.LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT,
          WindowManager.LayoutParams.TYPE_APPLICATION,
          WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE |
            WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE, PixelFormat.TRANSLUCENT))

    val config =
      new ImageLoaderConfiguration.Builder(this)
        .imageDownloader(new AppIconDownloader(this))
        .build()
    ImageLoader.getInstance().init(config)

    val bypassSwitch = findViewById(R.id.bypassSwitch).asInstanceOf[Switch]
    val prefs = PreferenceManager.getDefaultSharedPreferences(getBaseContext)
    bypassSwitch.setOnCheckedChangeListener(new OnCheckedChangeListener {
      def onCheckedChanged(button: CompoundButton, checked: Boolean) {
        prefs.edit().putBoolean(Key.isBypassApps, checked).commit()
      }
    })
    bypassSwitch.setChecked(prefs.getBoolean(Key.isBypassApps, false))

    appListView = findViewById(R.id.applistview).asInstanceOf[ListView]
  }

  protected override def onResume() {
    super.onResume()
    spawn {
      handler.post(loadStartRunnable)
      if (!appsLoaded) loadApps()
      handler.post(loadFinishRunnable)
    }
  }

  def saveAppSettings(context: Context) {
    if (apps == null) return
    val prefs = PreferenceManager.getDefaultSharedPreferences(this)
    val proxiedApps = new StringBuilder
    apps.foreach(app =>
      if (app.proxied) {
        proxiedApps ++= app.uid.toString
        proxiedApps += '|'
      })
    val edit: SharedPreferences.Editor = prefs.edit
    edit.putString(Key.proxied, proxiedApps.toString())
    edit.commit
  }

  val loadStartRunnable = new Runnable {
    override def run() {
      progressDialog = ProgressDialog
        .show(AppManager.this, "", getString(R.string.loading), true, true)
    }
  }

  val loadFinishRunnable = new Runnable {
    override def run() = {
      appListView.setAdapter(adapter)
      appListView.setOnScrollListener(new AbsListView.OnScrollListener {
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
        var visible = false
      })
      if (progressDialog != null) {
        progressDialog.dismiss()
        progressDialog = null
      }
    }
  }

  var handler: Handler = null

  class AppIconDownloader(context: Context, connectTimeout: Int, readTimeout: Int)
    extends BaseImageDownloader(context, connectTimeout, readTimeout) {

    def this(context: Context) {
      this(context, 0, 0)
    }

    override def getStreamFromOtherSource(imageUri: String, extra: AnyRef): InputStream = {
      val packageName = imageUri.substring(Scheme.APP.length)
      val drawable = Utils.getAppIcon(getBaseContext, packageName)
      val bitmap = Utils.drawableToBitmap(drawable)

      val os = new ByteArrayOutputStream()
      bitmap.compress(Bitmap.CompressFormat.PNG, 100, os)
      new ByteArrayInputStream(os.toByteArray)
    }
  }

}
