package com.github.shadowsocks.unrealvpn

import android.content.Context
import android.content.SharedPreferences

object UnrealVpnStore {

    fun setUnlimitedUntil(context: Context, newDate: Long) {
        prefs(context)
            .edit()
            .putLong(UNLIMITED, newDate)
            .apply()
    }

    fun getUnlimitedUntil(context: Context): Long {
        return prefs(context).getLong(UNLIMITED, 0)
    }


    fun getAccessUrl(context: Context): String? {
        return prefs(context).getString(ACCESS_URL, null)
    }

    fun setAccessUrl(context: Context, accessUrl: String) {
        prefs(context)
            .edit()
            .putString(ACCESS_URL, accessUrl)
            .apply()
    }

    fun getId(context: Context): String? {
        return prefs(context).getString(KEY_ID, null)
    }

    fun setId(context: Context, id: String) {
        prefs(context)
            .edit()
            .putString(KEY_ID, id)
            .apply()
    }

    private fun prefs(context: Context): SharedPreferences {
        return context.getSharedPreferences("UnrealVPN", Context.MODE_PRIVATE)
    }

    private const val KEY_ID = "keyid"
    private const val ACCESS_URL = "accessUrl"
    private const val UNLIMITED = "unlimited"
}
