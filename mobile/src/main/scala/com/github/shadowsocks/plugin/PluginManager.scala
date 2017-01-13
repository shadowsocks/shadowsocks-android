package com.github.shadowsocks.plugin

import android.content.pm.PackageManager
import android.content.{BroadcastReceiver, Intent}
import com.github.shadowsocks.ShadowsocksApplication.app

import scala.collection.JavaConverters._

/**
  * @author Mygod
  */
object PluginManager {
  private var receiver: BroadcastReceiver = _
  private var cachedPlugins: Array[Plugin] = _
  def fetchPlugins(): Array[Plugin] = {
    if (receiver == null) receiver = app.listenForPackageChanges(synchronized(cachedPlugins = null))
    synchronized(if (cachedPlugins == null) {
      val pm = app.getPackageManager
      cachedPlugins = (NoPlugin +:
        pm.queryIntentContentProviders(new Intent().addCategory(PluginInterface.CATEGORY_NATIVE_PLUGIN),
          PackageManager.GET_META_DATA).asScala.map(new NativePlugin(_, pm))).toArray
    })
    cachedPlugins
  }
}
