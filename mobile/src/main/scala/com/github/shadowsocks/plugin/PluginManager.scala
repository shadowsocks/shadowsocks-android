package com.github.shadowsocks.plugin

import java.io.{File, FileNotFoundException, FileOutputStream, IOException}

import android.content.pm.PackageManager
import android.content.{BroadcastReceiver, ContentResolver, Intent}
import android.net.Uri
import com.github.shadowsocks.ShadowsocksApplication.app
import com.github.shadowsocks.utils.CloseUtils.autoClose
import com.github.shadowsocks.utils.IOUtils

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

  // the following parts are meant to be used by :bg
  @throws[Throwable]
  def initPlugin(id: String): String = {
    if (id.isEmpty) return null
    var throwable: Throwable = null

    try initNativePlugin(id) match {
      case null =>
      case path => return path
    } catch {
      case t: Throwable =>
        t.printStackTrace()
        if (throwable == null) throwable = t
    }

    // add other plugin types here

    throw if (throwable == null)
      new FileNotFoundException(app.getString(com.github.shadowsocks.R.string.plugin_unknown, id)) else throwable
  }

  @throws[IOException]
  @throws[IndexOutOfBoundsException]
  @throws[AssertionError]
  private def initNativePlugin(id: String): String = {
    val builder = new Uri.Builder()
      .scheme(ContentResolver.SCHEME_CONTENT)
      .authority(PluginInterface.getAuthority(id))
    val cr = app.getContentResolver
    val cursor = cr.query(builder.build(), Array(PluginInterface.COLUMN_PATH), null, null, null)
    if (cursor != null) {
      var initialized = false
      val pluginDir = new File(app.getFilesDir, "plugin")
      if (cursor.moveToFirst()) {
        IOUtils.deleteRecursively(pluginDir)
        if (!pluginDir.mkdirs()) throw new FileNotFoundException("Unable to create plugin directory")
        val pluginDirPath = pluginDir.getAbsolutePath + '/'
        do {
          val path = cursor.getString(0)
          val file = new File(pluginDir, path)
          assert(file.getAbsolutePath.startsWith(pluginDirPath))
          autoClose(cr.openInputStream(builder.path(path).build()))(in =>
            autoClose(new FileOutputStream(file))(out => IOUtils.copy(in, out)))
          if (path == id) initialized = true
        } while (cursor.moveToNext())
      }
      if (!initialized) throw new IndexOutOfBoundsException("Plugin entry binary not found")
      new File(pluginDir, id).getAbsolutePath
    } else null
  }
}
