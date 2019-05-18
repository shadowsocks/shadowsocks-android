package com.github.shadowsocks.utils

import androidx.activity.ComponentActivity
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.LifecycleOwner
import java.util.*
import java.util.concurrent.ConcurrentHashMap

/**
 * See also: https://stackoverflow.com/a/30821062/2245107
 */
object SingleInstanceActivity : DefaultLifecycleObserver {
    private val active = Collections.newSetFromMap(ConcurrentHashMap<Class<LifecycleOwner>, Boolean>())

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
