package com.github.shadowsocks.plugin

import java.io.{File, FileNotFoundException, FileOutputStream, IOException}

import android.content.pm.PackageManager
import android.content.{BroadcastReceiver, ContentResolver, Intent}
import android.net.Uri
import android.os.Bundle
import android.util.Log
import com.github.shadowsocks.ShadowsocksApplication.app
import com.github.shadowsocks.utils.CloseUtils.autoClose
import com.github.shadowsocks.utils.{Commandline, IOUtils}
import eu.chainfire.libsuperuser.Shell

import scala.collection.JavaConversions._
import scala.collection.mutable.ListBuffer

/**
  * @author Mygod
  */
object PluginManager {
  private var receiver: BroadcastReceiver = _
  private var cachedPlugins: Map[String, Plugin] = _
  def fetchPlugins(): Map[String, Plugin] = {
    if (receiver == null) receiver = app.listenForPackageChanges(synchronized(cachedPlugins = null))
    synchronized(if (cachedPlugins == null) {
      val pm = app.getPackageManager
      cachedPlugins = (
        pm.queryIntentContentProviders(new Intent(PluginContract.ACTION_NATIVE_PLUGIN), PackageManager.GET_META_DATA)
          .map(new NativePlugin(_, pm)) :+
        NoPlugin
      ).map(plugin => plugin.id -> plugin).toMap
    })
    cachedPlugins
  }

  private def buildUri(id: String) = new Uri.Builder()
    .scheme(PluginContract.SCHEME)
    .authority(PluginContract.AUTHORITY)
    .path('/' + id)
    .build()
  def buildIntent(id: String, action: String): Intent = new Intent(action, buildUri(id))

  // the following parts are meant to be used by :bg
  @throws[Throwable]
  def init(options: PluginOptions): String = {
    if (options.id.isEmpty) return null
    var throwable: Throwable = null

    try initNative(options) match {
      case null =>
      case path => return path
    } catch {
      case t: Throwable =>
        t.printStackTrace()
        if (throwable == null) throwable = t
    }

    // add other plugin types here

    throw if (throwable == null) new FileNotFoundException(
      app.getString(com.github.shadowsocks.R.string.plugin_unknown, options.id)) else throwable
  }

  @throws[IOException]
  @throws[IndexOutOfBoundsException]
  @throws[AssertionError]
  private def initNative(options: PluginOptions): String = {
    val providers = app.getPackageManager.queryIntentContentProviders(
      new Intent(PluginContract.ACTION_NATIVE_PLUGIN, buildUri(options.id)), 0)
    assert(providers.length == 1)
    val uri = new Uri.Builder()
      .scheme(ContentResolver.SCHEME_CONTENT)
      .authority(providers(0).providerInfo.authority)
      .build()
    val cr = app.getContentResolver
    try initNativeFast(cr, options, uri) catch {
      case t: Throwable =>
        t.printStackTrace()
        Log.w("PluginManager", "Initializing native plugin fast mode failed. Falling back to slow mode.")
        initNativeSlow(cr, options, uri)
    }
  }

  private def initNativeFast(cr: ContentResolver, options: PluginOptions, uri: Uri): String = {
    val out = new Bundle()
    out.putString(PluginContract.EXTRA_OPTIONS, options.id)
    val result = cr.call(uri, PluginContract.METHOD_GET_EXECUTABLE, null, out).getString(PluginContract.EXTRA_ENTRY)
    assert(new File(result).canExecute)
    result
  }

  private def initNativeSlow(cr: ContentResolver, options: PluginOptions, uri: Uri): String =
    cr.query(uri, Array(PluginContract.COLUMN_PATH, PluginContract.COLUMN_MODE), null, null, null) match {
      case null => null
      case cursor =>
        var initialized = false
        val pluginDir = new File(app.getFilesDir, "plugin")
        def entryNotFound() = throw new IndexOutOfBoundsException("Plugin entry binary not found")
        if (!cursor.moveToFirst()) entryNotFound()
        IOUtils.deleteRecursively(pluginDir)
        if (!pluginDir.mkdirs()) throw new FileNotFoundException("Unable to create plugin directory")
        val pluginDirPath = pluginDir.getAbsolutePath + '/'
        val list = new ListBuffer[String]
        do {
          val path = cursor.getString(0)
          val file = new File(pluginDir, path)
          assert(file.getAbsolutePath.startsWith(pluginDirPath))
          autoClose(cr.openInputStream(uri.buildUpon().path(path).build()))(in =>
            autoClose(new FileOutputStream(file))(out => IOUtils.copy(in, out)))
          list += Commandline.toString(Array("chmod", cursor.getString(1), file.getAbsolutePath))
          if (path == options.id) initialized = true
        } while (cursor.moveToNext())
        if (!initialized) entryNotFound()
        Shell.SH.run(list)
        new File(pluginDir, options.id).getAbsolutePath
    }
}
