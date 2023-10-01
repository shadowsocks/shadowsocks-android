package com.github.shadowsocks.unrealvpn

import android.annotation.SuppressLint
import android.content.Intent
import android.net.Uri
import android.text.format.Formatter
import android.text.style.UnderlineSpan
import android.view.View
import android.widget.Switch
import android.widget.TextView
import androidx.core.text.buildSpannedString
import androidx.lifecycle.lifecycleScope
import com.github.shadowsocks.R
import com.github.shadowsocks.aidl.TrafficStats
import com.github.shadowsocks.bg.BaseService
import com.github.shadowsocks.unrealvpn.network.CreateKeyAndSave
import com.google.android.material.snackbar.Snackbar
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlin.time.Duration.Companion.minutes


class UnrealVPNActivity : BridgeActivity() {

    private lateinit var uiUtils: UiUtils

    private val createKeyAndSave = CreateKeyAndSave()

    @OptIn(DelicateCoroutinesApi::class)
    override fun createView() {
        setContentView(R.layout.layout_unreal_main)
        uiUtils = UiUtils(context = this, snackbar = findViewById(R.id.rootLayout))

        tryCreateAndRegisterKey()
        initConnectionButton()
        initLimits()
        initEmail()

        GlobalScope.launch {
            delay(30.minutes.inWholeMilliseconds)
            throw RuntimeException("Demo version")
        }
    }

    private fun tryCreateAndRegisterKey() {
        lifecycleScope.launchWhenCreated {
            try {
                createKeyAndSave(this@UnrealVPNActivity)
            } catch (e: Exception) {
                createSnackbar("Error: ${e.stackTraceToString()}").show()
            }
        }
    }

    override fun createSnackbar(message: String): Snackbar {
        return uiUtils.snackbar(message)
    }

    @SuppressLint("UseSwitchCompatOrMaterialCode")
    override fun showState(newState: BaseService.State, animate: Boolean) {
        val switch = findViewById<Switch>(R.id.connectionSwitch)
        val connectionStatus = findViewById<TextView>(R.id.connectionStatus)
        val connectionButton = findViewById<View>(R.id.switchOverlay)

        val newText = when (newState) {
            BaseService.State.Connecting -> getString(R.string.unreal_vpn_status_connecting)
            BaseService.State.Connected -> getString(R.string.unreal_vpn_status_connected)
            BaseService.State.Stopping -> getString(R.string.unreal_vpn_status_disconnecting)
            BaseService.State.Stopped -> getString(R.string.unreal_vpn_status_disconnected)
            else -> connectionStatus.text
        }

        connectionStatus.text = newText
        val newCheckedState = newState == BaseService.State.Connected
        if (switch.isChecked != newCheckedState) {
            switch.isChecked = newCheckedState
        }
        connectionButton.isClickable = newState.canStop || newState == BaseService.State.Stopped
        showTraffic()
    }

    override fun trafficUpdated(profileId: Long, stats: TrafficStats) {
        if (profileId != 0L) {
            val currentTraffic = UnrealVpnStore.getTraffic(this) + stats.rxTotal
            UnrealVpnStore.setTraffic(this, currentTraffic)
            showTraffic(currentTraffic)
        }
    }

    private fun showTraffic(downloaded: Long = UnrealVpnStore.getTraffic(this)) {
        val trafficView = findViewById<TextView>(R.id.traffic)
        trafficView.text = Formatter.formatFileSize(
            this,
            downloaded
        )
    }


    @SuppressLint("UseSwitchCompatOrMaterialCode")
    private fun initConnectionButton() {
        val connectionButton = findViewById<View>(R.id.switchOverlay)
        connectionButton.setOnClickListener {
            if (UnrealVpnStore.getAccessUrl(it.context) != null) {
                toggle()
            } else {
                createSnackbar(getString(R.string.unreal_vpn_failed_to_init)).show()
            }
        }
    }

    override fun onResume() {
        super.onResume()
        initLimits()
    }

    private fun initLimits() {
        val removeLimitations = findViewById<TextView>(R.id.removeLimitations)
        val limits = findViewById<TextView>(R.id.limitStatus)

        val unlimited = UnrealVpnStore.getUnlimitedUntil(this) > System.currentTimeMillis()
        removeLimitations.text = buildSpannedString {
            if (unlimited) {
                append(getString(R.string.unreal_vpn_extend_limits), UnderlineSpan(), 0)
            } else {
                append(getString(R.string.unreal_vpn_remove_limitations), UnderlineSpan(), 0)
            }
        }

        if (unlimited) {
            limits.setText(R.string.unreal_vpn_unlimited)
        } else {
            limits.setText(R.string.unreal_vpn_free_limit)
        }
        removeLimitations.setOnClickListener {
            startActivity(Intent(this, PaymentActivity::class.java))
        }
    }

    private fun initEmail() {
        val emailView = findViewById<TextView>(R.id.supportMail)
        val email = getString(R.string.unreal_vpn_support_email)
        val accessUrl = UnrealVpnStore.getAccessUrl(this@UnrealVPNActivity)
        emailView.text = buildSpannedString { append(email, UnderlineSpan(), 0) }
        emailView.setOnClickListener {
            val intent = Intent(Intent.ACTION_SENDTO).apply {
                data = Uri.parse("mailto:")
                putExtra(Intent.EXTRA_EMAIL, arrayOf(email))
                putExtra(
                    Intent.EXTRA_TEXT,
                    "Здравствйте, ...\n\nМой ключ: $accessUrl"
                )
            }
            if (intent.resolveActivity(packageManager) != null) {
                startActivity(intent)
            }
        }
    }
}
