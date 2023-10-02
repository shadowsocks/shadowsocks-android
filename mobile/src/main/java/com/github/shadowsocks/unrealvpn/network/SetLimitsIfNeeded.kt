package com.github.shadowsocks.unrealvpn.network

import android.annotation.SuppressLint
import android.content.Context
import android.util.Log
import com.github.shadowsocks.unrealvpn.UnrealVpnStore

class SetLimitsIfNeeded {

    private val unrealRestService = UnrealRestService()

    @SuppressLint("LogNotTimber")
    suspend operator fun invoke(context: Context) {
        val id = UnrealVpnStore.getId(context) ?: return
        val unlimitedUntil = UnrealVpnStore.getUnlimitedUntil(context)
        val shouldReset = unlimitedUntil != 0L
        val isNotUnlimited = unlimitedUntil < System.currentTimeMillis()
        if (shouldReset && isNotUnlimited) {
            Log.d(TAG, "Reset limits")
            unrealRestService.setLimits(id)
            UnrealVpnStore.setUnlimitedUntil(context, 0)
            Log.d(TAG, "Reset successfully")
        }
    }

    companion object {
        const val TAG = "SetLimitsIfNeeded"
    }
}
