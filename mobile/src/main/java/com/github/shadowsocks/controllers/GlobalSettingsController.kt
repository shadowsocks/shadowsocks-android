package com.github.shadowsocks.controllers


import android.os.Build
import android.os.Bundle
import android.support.design.widget.Snackbar
import android.support.v7.preference.Preference
import android.support.v7.preference.SwitchPreference
import android.util.Log
import android.view.View
import com.bluelinelabs.conductor.ControllerChangeHandler
import com.bluelinelabs.conductor.ControllerChangeType
import com.github.shadowsocks.App
import com.github.shadowsocks.BootReceiver
import com.github.shadowsocks.MainActivity
import im.mash.preference.PreferenceController

import com.github.shadowsocks.R
import com.github.shadowsocks.bg.BaseService
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.utils.DirectBoot
import com.github.shadowsocks.utils.Key
import com.github.shadowsocks.utils.TcpFastOpen

class GlobalSettingsController : PreferenceController() {

    companion object {
        const val TAG = "GlobalSettings"
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        preferenceManager!!.preferenceDataStore = DataStore.publicStore
        DataStore.initGlobal()
        addPreferencesFromResource(R.xml.pref_global)
        val boot = findPreference(Key.isAutoConnect) as SwitchPreference
        boot.setOnPreferenceChangeListener { _, value ->
            BootReceiver.enabled = value as Boolean
            true
        }
        boot.isChecked = BootReceiver.enabled

        val canToggleLocked = findPreference(Key.directBootAware)
        if (Build.VERSION.SDK_INT >= 24) canToggleLocked?.setOnPreferenceChangeListener { _, newValue ->
            if (App.app.directBootSupported && newValue as Boolean) DirectBoot.update() else DirectBoot.clean()
            true
        } else canToggleLocked?.parent!!.removePreference(canToggleLocked)

        val tfo = findPreference(Key.tfo) as SwitchPreference
        tfo.isChecked = TcpFastOpen.sendEnabled
        tfo.setOnPreferenceChangeListener { _, value ->
            val result = TcpFastOpen.enabled(value as Boolean)
            if (result != null && result != "Success.")
                Snackbar.make(activity!!.findViewById(R.id.snackbar), result, Snackbar.LENGTH_LONG).show()
            value == TcpFastOpen.sendEnabled
        }
        if (!TcpFastOpen.supported) {
            tfo.isEnabled = false
            tfo.summary = resources!!.getString(R.string.tcp_fastopen_summary_unsupported, System.getProperty("os.version"))
        }

        val serviceMode = findPreference(Key.serviceMode)
        val portProxy = findPreference(Key.portProxy)
        val portLocalDns = findPreference(Key.portLocalDns)
        val portTransproxy = findPreference(Key.portTransproxy)
        val onServiceModeChange = Preference.OnPreferenceChangeListener { _, newValue ->
            val (enabledLocalDns, enabledTransproxy) = when (newValue as String?) {
                Key.modeProxy -> Pair(false, false)
                Key.modeVpn -> Pair(true, false)
                Key.modeTransproxy -> Pair(true, true)
                else -> throw IllegalArgumentException()
            }
            portLocalDns?.isEnabled = enabledLocalDns
            portTransproxy?.isEnabled = enabledTransproxy
            true
        }
        val listener: (Int) -> Unit = {
            when (it) {
                BaseService.IDLE, BaseService.STOPPED -> {
                    serviceMode?.isEnabled = true
                    portProxy?.isEnabled = true
                    onServiceModeChange.onPreferenceChange(null, DataStore.serviceMode)
                }
                else -> {
                    serviceMode?.isEnabled = false
                    portProxy?.isEnabled = false
                    portLocalDns?.isEnabled = false
                    portTransproxy?.isEnabled = false
                }
            }
        }
        listener((activity as MainActivity).state)
        MainActivity.stateListener = listener
        serviceMode?.onPreferenceChangeListener = onServiceModeChange
    }

    override fun onChangeStarted(changeHandler: ControllerChangeHandler, changeType: ControllerChangeType) {
        super.onChangeStarted(changeHandler, changeType)
        if (changeType.isEnter) {
            (activity as MainActivity).toolbar.setTitle(R.string.settings)
        }
    }

    override fun onAttach(view: View) {
        (activity as MainActivity).toolbar.setTitle(R.string.settings)
        super.onAttach(view)
    }

    override fun onDestroy() {
        MainActivity.stateListener = null
        super.onDestroy()
    }

    override fun onCreateItemDecoration(): DividerDecoration {
        return CategoryDivideDividerDecoration()
    }
}