package com.github.shadowsocks.plugin

import java.io.{File, FileNotFoundException, FileOutputStream, IOException}

import android.content.pm.{PackageManager, Signature}
import android.content.{BroadcastReceiver, ContentResolver, Intent}
import android.net.Uri
import android.os.Bundle
import android.util.{Base64, Log}
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
  /**
    * Trusted signatures by the app. Third-party fork should add their public key to their fork if the developer wishes
    * to publish or has published plugins for this app. You can obtain your public key by executing:
    *
    * $ keytool -export -alias key-alias -keystore /path/to/keystore.jks -rfc
    *
    * If you don't plan to publish any plugin but is developing/has developed some, it's not necessary to add your
    * public key yet since it will also automatically trust packages signed by the same signatures, e.g. debug keys.
    */
  lazy val trustedSignatures: Set[Signature] = app.info.signatures.toSet +
    new Signature(Base64.decode(  // @Mygod
      """
        |MIIDWzCCAkOgAwIBAgIEUzfv8DANBgkqhkiG9w0BAQsFADBdMQswCQYDVQQGEwJD
        |TjEOMAwGA1UECBMFTXlnb2QxDjAMBgNVBAcTBU15Z29kMQ4wDAYDVQQKEwVNeWdv
        |ZDEOMAwGA1UECxMFTXlnb2QxDjAMBgNVBAMTBU15Z29kMCAXDTE0MDUwMjA5MjQx
        |OVoYDzMwMTMwOTAyMDkyNDE5WjBdMQswCQYDVQQGEwJDTjEOMAwGA1UECBMFTXln
        |b2QxDjAMBgNVBAcTBU15Z29kMQ4wDAYDVQQKEwVNeWdvZDEOMAwGA1UECxMFTXln
        |b2QxDjAMBgNVBAMTBU15Z29kMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKC
        |AQEAjm5ikHoP3w6zavvZU5bRo6Birz41JL/nZidpdww21q/G9APA+IiJMUeeocy0
        |L7/QY8MQZABVwNq79LXYWJBcmmFXM9xBPgDqQP4uh9JsvazCI9bvDiMn92mz9HiS
        |Sg9V4KGg0AcY0r230KIFo7hz+2QBp1gwAAE97myBfA3pi3IzJM2kWsh4LWkKQMfL
        |M6KDhpb4mdDQnHlgi4JWe3SYbLtpB6whnTqjHaOzvyiLspx1tmrb0KVxssry9KoX
        |YQzl56scfE/QJX0jJ5qYmNAYRCb4PibMuNSGB2NObDabSOMAdT4JLueOcHZ/x9tw
        |agGQ9UdymVZYzf8uqc+29ppKdQIDAQABoyEwHzAdBgNVHQ4EFgQUBK4uJ0cqmnho
        |6I72VmOVQMvVCXowDQYJKoZIhvcNAQELBQADggEBABZQ3yNESQdgNJg+NRIcpF9l
        |YSKZvrBZ51gyrC7/2ZKMpRIyXruUOIrjuTR5eaONs1E4HI/uA3xG1eeW2pjPxDnO
        |zgM4t7EPH6QbzibihoHw1MAB/mzECzY8r11PBhDQlst0a2hp+zUNR8CLbpmPPqTY
        |RSo6EooQ7+NBejOXysqIF1q0BJs8Y5s/CaTOmgbL7uPCkzArB6SS/hzXgDk5gw6v
        |wkGeOtzcj1DlbUTvt1s5GlnwBTGUmkbLx+YUje+n+IBgMbohLUDYBtUHylRVgMsc
        |1WS67kDqeJiiQZvrxvyW6CZZ/MIGI+uAkkj3DqJpaZirkwPgvpcOIrjZy0uFvQM=
      """, Base64.DEFAULT)) +
    new Signature(Base64.decode( // @madeye
      """
        |MIICQzCCAaygAwIBAgIETV9OhjANBgkqhkiG9w0BAQUFADBmMQswCQYDVQQGEwJjbjERMA8GA1UE
        |CBMIU2hhbmdoYWkxDzANBgNVBAcTBlB1ZG9uZzEUMBIGA1UEChMLRnVkYW4gVW5pdi4xDDAKBgNV
        |BAsTA1BQSTEPMA0GA1UEAxMGTWF4IEx2MB4XDTExMDIxOTA1MDA1NFoXDTM2MDIxMzA1MDA1NFow
        |ZjELMAkGA1UEBhMCY24xETAPBgNVBAgTCFNoYW5naGFpMQ8wDQYDVQQHEwZQdWRvbmcxFDASBgNV
        |BAoTC0Z1ZGFuIFVuaXYuMQwwCgYDVQQLEwNQUEkxDzANBgNVBAMTBk1heCBMdjCBnzANBgkqhkiG
        |9w0BAQEFAAOBjQAwgYkCgYEAq6lA8LqdeEI+es9SDX85aIcx8LoL3cc//iRRi+2mFIWvzvZ+bLKr
        |4Wd0rhu/iU7OeMm2GvySFyw/GdMh1bqh5nNPLiRxAlZxpaZxLOdRcxuvh5Nc5yzjM+QBv8ECmuvu
        |AOvvT3UDmA0AMQjZqSCmxWIxc/cClZ/0DubreBo2st0CAwEAATANBgkqhkiG9w0BAQUFAAOBgQAQ
        |Iqonxpwk2ay+Dm5RhFfZyG9SatM/JNFx2OdErU16WzuK1ItotXGVJaxCZv3u/tTwM5aaMACGED5n
        |AvHaDGCWynY74oDAopM4liF/yLe1wmZDu6Zo/7fXrH+T03LBgj2fcIkUfN1AA4dvnBo8XWAm9VrI
        |1iNuLIssdhDz3IL9Yg==
      """, Base64.DEFAULT))

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
