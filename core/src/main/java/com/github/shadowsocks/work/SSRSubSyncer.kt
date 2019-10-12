package com.github.shadowsocks.work

import android.content.Context
import android.util.Base64
import androidx.work.*
import com.github.shadowsocks.Core
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.database.ProfileManager
import com.github.shadowsocks.database.SSRSub
import com.github.shadowsocks.database.SSRSubManager
import com.github.shadowsocks.utils.printLog
import com.github.shadowsocks.utils.useCancellable
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL
import java.util.concurrent.TimeUnit

class SSRSubSyncer(context: Context, workerParams: WorkerParameters) : CoroutineWorker(context, workerParams) {
    companion object {
        private const val NAME = "ssrSubAllUpdate"

        fun enqueue() = WorkManager.getInstance(Core.deviceStorage).enqueueUniquePeriodicWork(
                NAME, ExistingPeriodicWorkPolicy.REPLACE,
                PeriodicWorkRequestBuilder<SSRSubSyncer>(1, TimeUnit.DAYS).run {
                    setConstraints(Constraints.Builder()
                            .setRequiredNetworkType(NetworkType.CONNECTED)
                            .setRequiresCharging(false)
                            .setRequiresBatteryNotLow(true)
                            .build())
                    build()
                })

        fun cancel() = WorkManager.getInstance(Core.deviceStorage).cancelUniqueWork(NAME)
    }

    override suspend fun doWork(): Result = try {
        SSRSubManager.updateAll()
        Result.success()
    } catch (e: IOException) {
        e.printStackTrace()
        if (runAttemptCount > 5) Result.failure() else Result.retry()
    }
}
