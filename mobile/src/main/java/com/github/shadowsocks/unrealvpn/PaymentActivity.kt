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
import timber.log.Timber
import java.util.concurrent.TimeUnit
import kotlin.math.max

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
        webview.getSettings().javaScriptEnabled = true
        webview.getSettings().javaScriptCanOpenWindowsAutomatically = true
        val url = "$baseUrl?key=$keyEncoded&key_id=$keyId"
        webview.loadUrl(url)
        Timber.d(url)

        val toolbar = findViewById<Toolbar>(R.id.toolbar)
        toolbar.setNavigationOnClickListener { onBackPressedDispatcher.onBackPressed() }
    }

    private fun onUrlChanged(url: String) {
        if (url.startsWith("https://yoomoney.ru/checkout/payments/v2/success")) {
            val oldUnlim = UnrealVpnStore.getUnlimitedUntil(this)
            val oldOrCurrent = max(oldUnlim, System.currentTimeMillis())
            val whenPremiumEnds = oldOrCurrent + PREMIUM_DURATION
            UnrealVpnStore.setUnlimitedUntil(
                this,
                whenPremiumEnds
            )
            scheduleStop(whenPremiumEnds)
            onBackPressedDispatcher.onBackPressed()
        }
    }

    private fun scheduleStop(whenPremiumEnds: Long) {
        WorkManager.getInstance(this)
            .enqueueUniqueWork(
                "StopWorker",
                ExistingWorkPolicy.REPLACE,
                OneTimeWorkRequest.Builder(StopWorker::class.java)
                    .setInitialDelay(
                        whenPremiumEnds - System.currentTimeMillis(),
                        TimeUnit.MILLISECONDS
                    )
                    .build()
            )
    }
}
