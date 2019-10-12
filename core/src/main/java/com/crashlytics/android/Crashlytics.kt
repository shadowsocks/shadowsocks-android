package com.crashlytics.android

import android.util.Log

class Crashlytics {
    companion object {
        fun logException(throwable: Throwable) {
            Log.e("shadowsocksRb", "Crashlytics logException:", throwable)
        }

        fun log(priority: Int, tag: String, msg: String?) {
            Log.println(priority, tag, msg ?: "null")
        }
    }
}
