package com.github.shadowsocks.utils

import androidx.activity.ComponentActivity
import androidx.annotation.MainThread
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.LifecycleOwner

/**
 * See also: https://stackoverflow.com/a/30821062/2245107
 */
object SingleInstanceActivity : DefaultLifecycleObserver {
    private val active = mutableSetOf<Class<LifecycleOwner>>()

    @MainThread
    fun register(activity: ComponentActivity) = if (active.add(activity.javaClass)) apply {
        activity.lifecycle.addObserver(this)
    } else {
        activity.finish()
        null
    }

    override fun onDestroy(owner: LifecycleOwner) {
        check(active.remove(owner.javaClass)) { "Double destroy?" }
    }
}
