package com.github.shadowsocks.utils

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.graphics.Rect
import android.os.Build
import android.support.v4.app.Fragment
import android.support.v4.app.FragmentManager
import android.support.v7.util.SortedList
import android.util.DisplayMetrics
import android.util.TypedValue
import android.view.Gravity
import android.view.View
import android.view.Window
import android.widget.Toast
import com.github.shadowsocks.App.Companion.app
import java.lang.reflect.InvocationTargetException
import java.net.InetAddress
import java.net.URLConnection

private val isNumericMethod by lazy { InetAddress::class.java.getMethod("isNumeric", String::class.java) }
private val parseNumericAddressMethod by lazy {
    InetAddress::class.java.getMethod("parseNumericAddress", String::class.java)
}
private val fieldChildFragmentManager by lazy {
    val field = Fragment::class.java.getDeclaredField("mChildFragmentManager")
    field.isAccessible = true
    field
}

fun String.isNumericAddress(): Boolean = isNumericMethod.invoke(null, this) as Boolean
fun String.parseNumericAddress(): InetAddress = try {
    parseNumericAddressMethod.invoke(null, this) as InetAddress
} catch (exc: InvocationTargetException) {
    throw exc.cause ?: exc
}

fun parsePort(str: String?, default: Int, min: Int = 1025): Int {
    val x = str?.toIntOrNull() ?: default
    return if (x < min || x > 65535) default else x
}

fun broadcastReceiver(callback: (Context, Intent) -> Unit): BroadcastReceiver = object : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) = callback(context, intent)
}

/**
 * Wrapper for kotlin.concurrent.thread that tracks uncaught exceptions.
 */
fun thread(start: Boolean = true, isDaemon: Boolean = false, contextClassLoader: ClassLoader? = null,
           name: String? = null, priority: Int = -1, block: () -> Unit): Thread {
    val thread = kotlin.concurrent.thread(false, isDaemon, contextClassLoader, name, priority, block)
    thread.setUncaughtExceptionHandler(app::track)
    if (start) thread.start()
    return thread
}

val URLConnection.responseLength: Long
    get() = if (Build.VERSION.SDK_INT >= 24) contentLengthLong else contentLength.toLong()

/**
 * Based on: http://stackoverflow.com/a/21026866/2245107
 */
fun Toast.position(view: View, window: Window, offsetX: Int = 0, offsetY: Int = 0): Toast {
    val rect = Rect()
    window.decorView.getWindowVisibleDisplayFrame(rect)
    val viewLocation = IntArray(2)
    view.getLocationInWindow(viewLocation)
    val metrics = DisplayMetrics()
    window.windowManager.defaultDisplay.getMetrics(metrics)
    val toastView = this.view
    toastView.measure(View.MeasureSpec.makeMeasureSpec(metrics.widthPixels, View.MeasureSpec.UNSPECIFIED),
            View.MeasureSpec.makeMeasureSpec(metrics.heightPixels, View.MeasureSpec.UNSPECIFIED))
    setGravity(Gravity.START or Gravity.TOP,
            viewLocation[0] - rect.left + (view.width - toastView.measuredWidth) / 2 + offsetX,
            viewLocation[1] - rect.top + view.height + offsetY)
    return this
}

fun Float.dp(): Float = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, this, app.resources.displayMetrics)

/**
 * Based on: https://stackoverflow.com/a/15656428/2245107
 */
var Fragment.childFragManager: FragmentManager?
    get() = childFragmentManager
    set(value) = fieldChildFragmentManager.set(this, value)

private class SortedListIterable<out T>(private val list: SortedList<T>) : Iterable<T> {
    override fun iterator(): Iterator<T> = SortedListIterator(list)
}
private class SortedListIterator<out T>(private val list: SortedList<T>) : Iterator<T> {
    private var count = 0
    override fun hasNext() = count < list.size()
    override fun next(): T = list[count++]
}
fun <T> SortedList<T>.asIterable(): Iterable<T> = SortedListIterable(this)
