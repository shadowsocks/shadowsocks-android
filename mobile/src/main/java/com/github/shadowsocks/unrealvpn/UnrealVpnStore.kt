package com.github.shadowsocks.unrealvpn

import android.content.Context
import android.content.SharedPreferences

object UnrealVpnStore {

    fun setUnlimitedUntil(context: Context, newDate: Long) {
        prefs(context)
            .edit()
            .putLong("unlimited", newDate)
            .apply()
    }

    fun getUnlimitedUntil(context: Context): Long {
        return prefs(context).getLong("unlimited", 0)
    }


    fun getAccessUrl(context: Context): String? {
        return prefs(context).getString("accessUrl", null)
    }

    fun setAccessUrl(context: Context, accessUrl: String) {
        prefs(context)
            .edit()
            .putString("accessUrl", accessUrl)
            .apply()
    }

    fun getTraffic(context: Context): Long {
        return prefs(context).getLong("traffic", 0)
    }

    fun setTraffic(context: Context, traffic: Long) {
        prefs(context)
            .edit()
            .putLong("traffic", traffic)
            .apply()
    }

    private fun prefs(context: Context): SharedPreferences {
        return context.getSharedPreferences("UnrealVPN", Context.MODE_PRIVATE)
    }

}
