package com.github.shadowsocks.unrealvpn

import android.annotation.SuppressLint
import android.content.Intent
import android.net.Uri
import android.text.style.UnderlineSpan
import android.widget.Switch
import android.widget.TextView
import androidx.core.text.buildSpannedString
import com.github.shadowsocks.R
import com.github.shadowsocks.bg.BaseService
import com.google.android.material.snackbar.Snackbar


class UnrealVPNActivity : BridgeActivity() {

    private lateinit var uiUtils: UiUtils

    override fun createView() {
        setContentView(R.layout.layout_unreal_main)
        uiUtils = UiUtils(context = this, snackbar = findViewById(R.id.rootLayout))

        initConnectionButton()
        initLimits()
        initEmail()
    }

    override fun createSnackbar(message: String): Snackbar {
        return uiUtils.snackbar(message)
    }

    @SuppressLint("UseSwitchCompatOrMaterialCode")
    override fun showState(state: BaseService.State, animate: Boolean) {
        val switch = findViewById<Switch>(R.id.connectionSwitch)
        val connectionStatus = findViewById<TextView>(R.id.connectionStatus)

        val newText = when (state) {
            BaseService.State.Connecting -> getString(R.string.unreal_vpn_status_connecting)
            BaseService.State.Connected -> getString(R.string.unreal_vpn_status_connected)
            BaseService.State.Stopping -> getString(R.string.unreal_vpn_status_disconnecting)
            BaseService.State.Stopped -> getString(R.string.unreal_vpn_status_disconnected)
            else -> connectionStatus.text
        }
        connectionStatus.text = newText
        switch.isChecked = state == BaseService.State.Connected
        switch.isEnabled = state.canStop || state == BaseService.State.Stopped
    }

    fun getId(): String = "asdasdasd"

    private fun initConnectionButton() {
        val connectionStatus = findViewById<TextView>(R.id.connectionStatus)
        connectionStatus.setOnClickListener { toggle() }
    }

    override fun onResume() {
        super.onResume()
        initLimits()
    }

    private fun initLimits() {
        val removeLimitations = findViewById<TextView>(R.id.removeLimitations)
        val limits = findViewById<TextView>(R.id.limitStatus)
        if (UnrealVpnStore.unlimited) {
            limits.setText(R.string.unreal_vpn_unlimited)
        } else {
            limits.setText(R.string.unreal_vpn_free_limit)
        }
        removeLimitations.setOnClickListener {
            uiUtils.launchUrl(getString(R.string.unreal_vpn_payment_url))
        }
    }

    private fun initEmail() {
        val emailView = findViewById<TextView>(R.id.supportMail)
        val email = getString(R.string.unreal_vpn_support_email)
        emailView.text = buildSpannedString { append(email, UnderlineSpan(), 0) }
        emailView.setOnClickListener {
            val intent = Intent(Intent.ACTION_SENDTO).apply {
                data = Uri.parse("mailto:")
                putExtra(Intent.EXTRA_EMAIL, arrayOf(email))
                putExtra(Intent.EXTRA_TEXT, "Здравствйте, ...\n\nМой идентификатор: ${getId()}")
            }
            if (intent.resolveActivity(packageManager) != null) {
                startActivity(intent)
            }
        }
    }
}
