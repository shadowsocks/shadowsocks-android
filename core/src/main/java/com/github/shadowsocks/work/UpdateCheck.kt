package com.github.shadowsocks.work

import android.app.NotificationManager
import android.content.Context
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat
import androidx.core.content.getSystemService
import androidx.work.*
import com.github.shadowsocks.Core
import com.github.shadowsocks.Core.app
import com.github.shadowsocks.core.BuildConfig
import com.github.shadowsocks.core.R
import com.github.shadowsocks.utils.useCancellable
import com.google.gson.JsonStreamParser
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL
import java.util.concurrent.TimeUnit


class UpdateCheck(context: Context, workerParams: WorkerParameters) : CoroutineWorker(context, workerParams) {
    companion object {
        fun enqueue() = WorkManager.getInstance(Core.deviceStorage).enqueueUniquePeriodicWork(
                "UpdateCheck", ExistingPeriodicWorkPolicy.REPLACE,
                PeriodicWorkRequestBuilder<UpdateCheck>(1, TimeUnit.DAYS).run {
                    setConstraints(Constraints.Builder()
                            .setRequiredNetworkType(NetworkType.UNMETERED)
                            .setRequiresCharging(false)
                            .build())
                    build()
                })
    }

    override suspend fun doWork(): Result = try {
        val connection = URL("").openConnection() as HttpURLConnection
        val json = connection.useCancellable { inputStream.bufferedReader() }
        val info = JsonStreamParser(json).asSequence().single().asJsonObject
        if (info["version"].asInt > BuildConfig.VERSION_CODE) {
            val nm = app.getSystemService<NotificationManager>()!!
            val builder = NotificationCompat.Builder(app as Context, "update")
                    .setColor(ContextCompat.getColor(app, R.color.material_primary_500))
                    .setSmallIcon(R.drawable.ic_service_active)
                    .setCategory(NotificationCompat.CATEGORY_STATUS)
                    .setContentTitle(info["title"].toString())
                    .setContentText(info["text"].toString())
            nm.notify(62, builder.build())
        }
        Result.success()
    } catch (e: Exception) {
        e.printStackTrace()
        if (runAttemptCount > 5) Result.failure() else Result.retry()
    }
}
