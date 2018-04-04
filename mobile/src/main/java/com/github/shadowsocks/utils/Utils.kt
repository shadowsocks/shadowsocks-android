package com.github.shadowsocks.utils

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.res.Resources
import android.os.Build
import android.support.annotation.AttrRes
import android.support.v4.app.Fragment
import android.support.v4.app.FragmentManager
import android.support.v7.util.SortedList
import android.util.TypedValue
import com.github.shadowsocks.App.Companion.app
import com.github.shadowsocks.JniHelper
import java.net.InetAddress
import java.net.URLConnection

private val fieldChildFragmentManager by lazy {
    val field = Fragment::class.java.getDeclaredField("mChildFragmentManager")
    field.isAccessible = true
    field
}

fun String.isNumericAddress() = JniHelper.parseNumericAddress(this) != null
fun String.parseNumericAddress(): InetAddress? {
    val addr = JniHelper.parseNumericAddress(this)
    return if (addr == null) null else InetAddress.getByAddress(this, addr)
}

fun parsePort(str: String?, default: Int, min: Int = 1025): Int {
    val value = str?.toIntOrNull() ?: default
    return if (value < min || value > 65535) default else value
}

fun parseTime(str: String?, default: Int, min: Int = 10): Int {
    val value = str?.toIntOrNull() ?: default
    return if (value < min || value > 3600) default else value
}

fun broadcastReceiver(callback: (Context, Intent) -> Unit): BroadcastReceiver = object : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) = callback(context, intent)
}

/**
 * Wrapper for kotlin.concurrent.thread that tracks uncaught exceptions.
 */
fun thread(name: String? = null, start: Boolean = true, isDaemon: Boolean = false,
           contextClassLoader: ClassLoader? = null, priority: Int = -1, block: () -> Unit): Thread {
    val thread = kotlin.concurrent.thread(false, isDaemon, contextClassLoader, name, priority, block)
    thread.setUncaughtExceptionHandler(app::track)
    if (start) thread.start()
    return thread
}

val URLConnection.responseLength: Long
    get() = if (Build.VERSION.SDK_INT >= 24) contentLengthLong else contentLength.toLong()

/**
 * Based on: https://stackoverflow.com/a/15656428/2245107
 */
var Fragment.childFragManager: FragmentManager?
    get() = childFragmentManager
    set(value) = fieldChildFragmentManager.set(this, value)

/**
 * Based on: https://stackoverflow.com/a/26348729/2245107
 */
fun Resources.Theme.resolveResourceId(@AttrRes resId: Int): Int {
    val typedValue = TypedValue()
    if (!resolveAttribute(resId, typedValue, true)) throw Resources.NotFoundException()
    return typedValue.resourceId
}

private class SortedListIterable<out T>(private val list: SortedList<T>) : Iterable<T> {
    override fun iterator(): Iterator<T> = SortedListIterator(list)
}
private class SortedListIterator<out T>(private val list: SortedList<T>) : Iterator<T> {
    private var count = 0
    override fun hasNext() = count < list.size()
    override fun next(): T = if (hasNext()) list[count++] else throw NoSuchElementException()
}
fun <T> SortedList<T>.asIterable(): Iterable<T> = SortedListIterable(this)
