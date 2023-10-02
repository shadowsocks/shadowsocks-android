package com.github.shadowsocks.unrealvpn

import android.annotation.SuppressLint
import android.content.Context
import android.util.Log
import androidx.work.Worker
import androidx.work.WorkerParameters
import com.github.shadowsocks.Core
import com.github.shadowsocks.unrealvpn.network.SetLimitsIfNeeded
import kotlinx.coroutines.runBlocking

class StopWorker(context: Context, params: WorkerParameters) :
    Worker(context.applicationContext, params) {

    private val setLimitsIfNeeded = SetLimitsIfNeeded()

    @SuppressLint("LogNotTimber")
    override fun doWork(): Result {
        return runBlocking {
            Log.d("StopWorker", "Start worker")
            setLimitsIfNeeded(applicationContext)
            Core.stopService()
            Log.d("StopWorker", "End worker")
            Result.success()
        }
    }
}
