/*******************************************************************************
 *                                                                             *
 *  Copyright (C) 2018 by Max Lv <max.c.lv@gmail.com>                          *
 *  Copyright (C) 2018 by Mygod Studio <contact-shadowsocks-android@mygod.be>  *
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

package com.github.shadowsocks.utils

import android.annotation.SuppressLint
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageInfo
import android.content.res.Resources
import android.os.Build
import android.system.Os
import android.system.OsConstants
import android.util.TypedValue
import androidx.annotation.AttrRes
import androidx.preference.Preference
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.suspendCancellableCoroutine
import timber.log.Timber
import java.io.FileDescriptor
import java.net.HttpURLConnection
import java.net.InetAddress
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

inline fun <T> Iterable<T>.forEachTry(action: (T) -> Unit) {
    var result: Exception? = null
    for (element in this) try {
        action(element)
    } catch (e: Exception) {
        if (result == null) result = e else result.addSuppressed(e)
    }
    if (result != null) {
        Timber.d(result)
        throw result
    }
}

val Throwable.readableMessage get() = localizedMessage ?: javaClass.name

/**
 * https://android.googlesource.com/platform/prebuilts/runtime/+/94fec32/appcompat/hiddenapi-light-greylist.txt#9466
 */
private val getInt = FileDescriptor::class.java.getDeclaredMethod("getInt$")
val FileDescriptor.int get() = getInt.invoke(this) as Int

private val parseNumericAddress by lazy @SuppressLint("SoonBlockedPrivateApi") {
    InetAddress::class.java.getDeclaredMethod("parseNumericAddress", String::class.java).apply {
        isAccessible = true
    }
}
/**
 * A slightly more performant variant of parseNumericAddress.
 *
 * Bug in Android 9.0 and lower: https://issuetracker.google.com/issues/123456213
 */
fun String?.parseNumericAddress(): InetAddress? = Os.inet_pton(OsConstants.AF_INET, this)
        ?: Os.inet_pton(OsConstants.AF_INET6, this)?.let {
            if (Build.VERSION.SDK_INT >= 29) it else parseNumericAddress.invoke(null, this) as InetAddress
        }

suspend fun <T> HttpURLConnection.useCancellable(block: suspend HttpURLConnection.() -> T): T {
    return suspendCancellableCoroutine { cont ->
        val job = GlobalScope.launch(Dispatchers.IO) {
            try {
                cont.resume(block())
            } catch (e: Throwable) {
                cont.resumeWithException(e)
            }
        }
        cont.invokeOnCancellation {
            job.cancel(it as? CancellationException)
            if (Build.VERSION.SDK_INT >= 26) disconnect() else GlobalScope.launch(Dispatchers.IO) { disconnect() }
        }
    }
}

fun parsePort(str: String?, default: Int, min: Int = 1025): Int {
    val value = str?.toIntOrNull() ?: default
    return if (value < min || value > 65535) default else value
}

fun broadcastReceiver(callback: (Context, Intent) -> Unit): BroadcastReceiver = object : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) = callback(context, intent)
}

fun Context.listenForPackageChanges(onetime: Boolean = true, callback: () -> Unit) = object : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        callback()
        if (onetime) context.unregisterReceiver(this)
    }
}.apply {
    registerReceiver(this, IntentFilter().apply {
        addAction(Intent.ACTION_PACKAGE_ADDED)
        addAction(Intent.ACTION_PACKAGE_REMOVED)
        addDataScheme("package")
    })
}

val PackageInfo.signaturesCompat get() =
    if (Build.VERSION.SDK_INT >= 28) signingInfo.apkContentsSigners else @Suppress("DEPRECATION") signatures

/**
 * Based on: https://stackoverflow.com/a/26348729/2245107
 */
fun Resources.Theme.resolveResourceId(@AttrRes resId: Int): Int {
    val typedValue = TypedValue()
    if (!resolveAttribute(resId, typedValue, true)) throw Resources.NotFoundException()
    return typedValue.resourceId
}

fun Preference.remove() = parent!!.removePreference(this)
