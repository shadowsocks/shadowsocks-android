package com.crashlytics.android

import android.util.Log
import com.github.shadowsocks.core.BuildConfig

class Crashlytics {
    companion object {
        fun logException(throwable: Throwable) {
            Log.e("shadowsocksRb", "Crashlytics logException:", throwable)
        }

        fun log(priority: Int, tag: String, msg: String?) {
            if (priority == Log.DEBUG && !BuildConfig.DEBUG) return
            Log.println(priority, tag, msg ?: "null")
        }
    }
}
