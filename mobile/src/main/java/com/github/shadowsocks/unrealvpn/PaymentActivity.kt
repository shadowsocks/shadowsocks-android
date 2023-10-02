package com.github.shadowsocks.unrealvpn

import android.annotation.SuppressLint
import android.os.Bundle
import android.webkit.WebView
import android.webkit.WebViewClient
import android.widget.Toolbar
import androidx.appcompat.app.AppCompatActivity
import androidx.work.ExistingWorkPolicy
import androidx.work.OneTimeWorkRequest
import androidx.work.WorkManager
import com.github.shadowsocks.R
import java.util.concurrent.TimeUnit
import kotlin.math.max
import kotlin.time.Duration.Companion.minutes

class PaymentActivity : AppCompatActivity() {

    private val listenerClient = object : WebViewClient() {
        override fun doUpdateVisitedHistory(view: WebView?, url: String?, isReload: Boolean) {
            onUrlChanged(url.orEmpty())
            super.doUpdateVisitedHistory(view, url, isReload)
        }
    }

    @SuppressLint("SetJavaScriptEnabled")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_payment)

        val webview = findViewById<WebView>(R.id.webview)
        val baseUrl = getString(R.string.unreal_vpn_payment_url)
        val keyEncoded = UnrealVpnStore.getAccessUrl(this)
        val keyId = UnrealVpnStore.getId(this)

        webview.webViewClient = listenerClient
        webview.getSettings().javaScriptEnabled = true;
        webview.getSettings().javaScriptCanOpenWindowsAutomatically = true;
        webview.loadUrl("$baseUrl?key=$keyEncoded&key_id=$keyId")

        val toolbar = findViewById<Toolbar>(R.id.toolbar)
        toolbar.setNavigationOnClickListener { onBackPressedDispatcher.onBackPressed() }
    }

    private val oneMonth = 30.minutes.inWholeMilliseconds

    private fun onUrlChanged(url: String) {
        if (url.startsWith("https://yoomoney.ru/checkout/payments/v2/success")) {
            val oldUnlim = UnrealVpnStore.getUnlimitedUntil(this)
            val oldOrCurrent = max(oldUnlim, System.currentTimeMillis())
            UnrealVpnStore.setUnlimitedUntil(
                this,
                oldOrCurrent + oneMonth
            )
            scheduleStop()
            onBackPressedDispatcher.onBackPressed()
        }
    }

    private fun scheduleStop() {
        WorkManager.getInstance(this)
            .enqueueUniqueWork(
                "StopWorker",
                ExistingWorkPolicy.REPLACE,
                OneTimeWorkRequest.Builder(StopWorker::class.java)
                    .setInitialDelay(oneMonth, TimeUnit.MILLISECONDS)
                    .build()
            )
    }
}
