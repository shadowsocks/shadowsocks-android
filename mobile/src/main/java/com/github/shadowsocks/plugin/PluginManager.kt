/*******************************************************************************
 *                                                                             *
 *  Copyright (C) 2017 by Max Lv <max.c.lv@gmail.com>                          *
 *  Copyright (C) 2017 by Mygod Studio <contact-shadowsocks-android@mygod.be>  *
 *                                                                             *
 *  This program is free software: you can redistribute it and/or modify       *
 *  it under the terms of the GNU General Public License as published by       *
 *  the Free Software Foundation, either version 3 of the License, or          *
 *  (at your option) any later version.                                        *
 *                                                                             *
 *  This program is distributed in the hope that it will be useful,            *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 *  GNU General Public License for more details.                               *
 *                                                                             *
 *  You should have received a copy of the GNU General Public License          *
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.       *
 *                                                                             *
 *******************************************************************************/

package com.github.shadowsocks.plugin

import android.annotation.SuppressLint
import android.content.BroadcastReceiver
import android.content.ContentResolver
import android.content.Intent
import android.content.pm.PackageManager
import android.content.pm.Signature
import android.net.Uri
import android.util.Base64
import android.util.Log
import androidx.core.os.bundleOf
import com.crashlytics.android.Crashlytics
import com.github.shadowsocks.App.Companion.app
import com.github.shadowsocks.utils.Commandline
import com.github.shadowsocks.utils.signaturesCompat
import eu.chainfire.libsuperuser.Shell
import java.io.File
import java.io.FileNotFoundException

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
    val trustedSignatures by lazy {
        app.info.signaturesCompat.toSet() +
                Signature(Base64.decode(  // @Mygod
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
                Signature(Base64.decode( // @madeye
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
    }

    private var receiver: BroadcastReceiver? = null
    private var cachedPlugins: Map<String, Plugin>? = null
    fun fetchPlugins(): Map<String, Plugin> = synchronized(this) {
        if (receiver == null) receiver = app.listenForPackageChanges {
            synchronized(this) {
                receiver = null
                cachedPlugins = null
            }
        }
        if (cachedPlugins == null) {
            val pm = app.packageManager
            cachedPlugins = (pm.queryIntentContentProviders(Intent(PluginContract.ACTION_NATIVE_PLUGIN),
                    PackageManager.GET_META_DATA).map { NativePlugin(it) } + NoPlugin).associate { it.id to it }
        }
        cachedPlugins!!
    }

    private fun buildUri(id: String) = Uri.Builder()
            .scheme(PluginContract.SCHEME)
            .authority(PluginContract.AUTHORITY)
            .path('/' + id)
            .build()
    fun buildIntent(id: String, action: String): Intent = Intent(action, buildUri(id))

    // the following parts are meant to be used by :bg
    @Throws(Throwable::class)
    fun init(options: PluginOptions): String? {
        if (options.id.isEmpty()) return null
        var throwable: Throwable? = null

        try {
            val path = initNative(options)
            if (path != null) return path
        } catch (t: Throwable) {
            t.printStackTrace()
            if (throwable == null) throwable = t
        }

        // add other plugin types here

        throw if (throwable != null) throwable else
            FileNotFoundException(app.getString(com.github.shadowsocks.R.string.plugin_unknown, options.id))
    }

    private fun initNative(options: PluginOptions): String? {
        val providers = app.packageManager.queryIntentContentProviders(
                Intent(PluginContract.ACTION_NATIVE_PLUGIN, buildUri(options.id)), 0)
        if (providers.isEmpty()) return null
        val uri = Uri.Builder()
                .scheme(ContentResolver.SCHEME_CONTENT)
                .authority(providers.single().providerInfo.authority)
                .build()
        val cr = app.contentResolver
        return try {
            initNativeFast(cr, options, uri)
        } catch (t: Throwable) {
            t.printStackTrace()
            Crashlytics.log(Log.WARN, "PluginManager",
                    "Initializing native plugin fast mode failed. Falling back to slow mode.")
            initNativeSlow(cr, options, uri)
        }
    }

    private fun initNativeFast(cr: ContentResolver, options: PluginOptions, uri: Uri): String {
        val result = cr.call(uri, PluginContract.METHOD_GET_EXECUTABLE, null,
                bundleOf(Pair(PluginContract.EXTRA_OPTIONS, options.id))).getString(PluginContract.EXTRA_ENTRY)
        check(File(result).canExecute())
        return result
    }

    @SuppressLint("Recycle")
    private fun initNativeSlow(cr: ContentResolver, options: PluginOptions, uri: Uri): String? {
        var initialized = false
        fun entryNotFound(): Nothing = throw IndexOutOfBoundsException("Plugin entry binary not found")
        val list = ArrayList<String>()
        val pluginDir = File(app.deviceContext.filesDir, "plugin")
        (cr.query(uri, arrayOf(PluginContract.COLUMN_PATH, PluginContract.COLUMN_MODE), null, null, null)
                ?: return null).use { cursor ->
            if (!cursor.moveToFirst()) entryNotFound()
            pluginDir.deleteRecursively()
            if (!pluginDir.mkdirs()) throw FileNotFoundException("Unable to create plugin directory")
            val pluginDirPath = pluginDir.absolutePath + '/'
            do {
                val path = cursor.getString(0)
                val file = File(pluginDir, path)
                check(file.absolutePath.startsWith(pluginDirPath))
                cr.openInputStream(uri.buildUpon().path(path).build()).use { inStream ->
                    file.outputStream().use { outStream -> inStream.copyTo(outStream) }
                }
                list += Commandline.toString(arrayOf("chmod", cursor.getString(1), file.absolutePath))
                if (path == options.id) initialized = true
            } while (cursor.moveToNext())
        }
        if (!initialized) entryNotFound()
        Shell.SH.run(list)
        return File(pluginDir, options.id).absolutePath
    }
}
