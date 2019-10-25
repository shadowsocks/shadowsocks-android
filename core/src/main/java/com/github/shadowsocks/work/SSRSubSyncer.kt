package com.github.shadowsocks.work

import android.content.Context
import androidx.work.*
import com.github.shadowsocks.Core
import com.github.shadowsocks.database.SSRSubManager
import com.github.shadowsocks.utils.printLog
import java.io.IOException
import java.util.concurrent.TimeUnit

class SSRSubSyncer(context: Context, workerParams: WorkerParameters) : CoroutineWorker(context, workerParams) {
    companion object {
        private const val NAME = "ssrSubAllUpdate"

        fun enqueue() = WorkManager.getInstance(Core.deviceStorage).enqueueUniquePeriodicWork(
                NAME, ExistingPeriodicWorkPolicy.KEEP,
                PeriodicWorkRequestBuilder<SSRSubSyncer>(1, TimeUnit.DAYS).run {
                    setBackoffCriteria(BackoffPolicy.EXPONENTIAL, 1, TimeUnit.MINUTES)
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
        printLog(e)
        if (runAttemptCount > 5) {
            cancel()
            Result.failure()
        } else Result.retry()
    }
}
