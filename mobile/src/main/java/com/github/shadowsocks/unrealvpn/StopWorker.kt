package com.github.shadowsocks.unrealvpn

import android.content.Context
import androidx.work.Worker
import androidx.work.WorkerParameters
import com.github.shadowsocks.Core

class StopWorker(context: Context, params: WorkerParameters) :
    Worker(context.applicationContext, params) {

    override fun doWork(): Result {
        Core.stopService()
        return Result.success()
    }
}
