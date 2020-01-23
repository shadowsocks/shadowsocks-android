/*******************************************************************************
 *                                                                             *
 *  Copyright (C) 2017 by Max Lv <max.c.lv@gmail.com>                          *
 *  Copyright (C) 2017 by Mygod Studio <contact-shadowsocks-android@mygod.be>  *
 *                                                                             *
 *  This program is free software: you can redistribute it and/or modify       *
 *  it under the terms of the GNU General Public License as published by       *
 *  the Free Software Foundation, either version 3 of the License, or          *
 *  (at your option) any later version.                                        *
 *                                                                             *
 *  This program is distributed in the hope that it will be useful,            *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 *  GNU General Public License for more details.                               *
 *                                                                             *
 *  You should have received a copy of the GNU General Public License          *
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.       *
 *                                                                             *
 *******************************************************************************/

package com.github.shadowsocks

import android.app.Activity
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.view.View
import androidx.preference.EditTextPreference
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreference
import com.github.shadowsocks.bg.BaseService
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.utils.DirectBoot
import com.github.shadowsocks.utils.Key
import com.github.shadowsocks.net.TcpFastOpen
import com.github.shadowsocks.plugin.showAllowingStateLoss
import com.github.shadowsocks.preference.BrowsableEditTextPreferenceDialogFragment
import com.github.shadowsocks.preference.EditTextPreferenceModifiers
import com.github.shadowsocks.preference.HostsSummaryProvider
import com.github.shadowsocks.utils.readableMessage
import com.github.shadowsocks.utils.remove
import com.github.shadowsocks.widget.MainListListener

class GlobalSettingsPreferenceFragment : PreferenceFragmentCompat() {
    companion object {
        private const val REQUEST_BROWSE = 1
    }

    private val hosts by lazy { findPreference<EditTextPreference>(Key.hosts)!! }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        preferenceManager.preferenceDataStore = DataStore.publicStore
        DataStore.initGlobal()
        addPreferencesFromResource(R.xml.pref_global)
        findPreference<SwitchPreference>(Key.persistAcrossReboot)!!.setOnPreferenceChangeListener { _, value ->
            BootReceiver.enabled = value as Boolean
            true
        }

        val canToggleLocked = findPreference<Preference>(Key.directBootAware)!!
        if (Build.VERSION.SDK_INT >= 24) canToggleLocked.setOnPreferenceChangeListener { _, newValue ->
            if (Core.directBootSupported && newValue as Boolean) DirectBoot.update() else DirectBoot.clean()
            true
        } else canToggleLocked.remove()

        val tfo = findPreference<SwitchPreference>(Key.tfo)!!
        tfo.isChecked = DataStore.tcpFastOpen
        tfo.setOnPreferenceChangeListener { _, value ->
            if (value as Boolean && !TcpFastOpen.sendEnabled) {
                val result = TcpFastOpen.enable()?.trim()
                if (TcpFastOpen.sendEnabled) true else {
                    (activity as MainActivity).snackbar(
                            if (result.isNullOrEmpty()) getText(R.string.tcp_fastopen_failure) else result).show()
                    false
                }
            } else true
        }
        if (!TcpFastOpen.supported) {
            tfo.isEnabled = false
            tfo.summary = getString(R.string.tcp_fastopen_summary_unsupported, System.getProperty("os.version"))
        }

        hosts.setOnBindEditTextListener(EditTextPreferenceModifiers.Monospace)
        hosts.summaryProvider = HostsSummaryProvider
        val serviceMode = findPreference<Preference>(Key.serviceMode)!!
        val portProxy = findPreference<EditTextPreference>(Key.portProxy)!!
        portProxy.setOnBindEditTextListener(EditTextPreferenceModifiers.Port)
        val portLocalDns = findPreference<EditTextPreference>(Key.portLocalDns)!!
        portLocalDns.setOnBindEditTextListener(EditTextPreferenceModifiers.Port)
        val portTransproxy = findPreference<EditTextPreference>(Key.portTransproxy)!!
        portTransproxy.setOnBindEditTextListener(EditTextPreferenceModifiers.Port)
        val onServiceModeChange = Preference.OnPreferenceChangeListener { _, newValue ->
            val (enabledLocalDns, enabledTransproxy) = when (newValue as String?) {
                Key.modeProxy -> Pair(false, false)
                Key.modeVpn -> Pair(true, false)
                Key.modeTransproxy -> Pair(true, true)
                else -> throw IllegalArgumentException("newValue: $newValue")
            }
            hosts.isEnabled = enabledLocalDns
            portLocalDns.isEnabled = enabledLocalDns
            portTransproxy.isEnabled = enabledTransproxy
            true
        }
        val listener: (BaseService.State) -> Unit = {
            val stopped = it == BaseService.State.Stopped
            tfo.isEnabled = stopped
            serviceMode.isEnabled = stopped
            portProxy.isEnabled = stopped
            if (stopped) onServiceModeChange.onPreferenceChange(null, DataStore.serviceMode) else {
                hosts.isEnabled = false
                portLocalDns.isEnabled = false
                portTransproxy.isEnabled = false
            }
        }
        listener((activity as MainActivity).state)
        MainActivity.stateListener = listener
        serviceMode.onPreferenceChangeListener = onServiceModeChange
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        listView.setOnApplyWindowInsetsListener(MainListListener)
    }

    override fun onDisplayPreferenceDialog(preference: Preference?) {
        if (preference == hosts) BrowsableEditTextPreferenceDialogFragment().apply {
            setKey(hosts.key)
            setTargetFragment(this@GlobalSettingsPreferenceFragment, REQUEST_BROWSE)
        }.showAllowingStateLoss(parentFragmentManager, hosts.key) else super.onDisplayPreferenceDialog(preference)
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        when (requestCode) {
            REQUEST_BROWSE -> {
                if (resultCode != Activity.RESULT_OK) return
                val activity = activity as MainActivity
                try {
                    // we read and persist all its content here to avoid content URL permission issues
                    hosts.text = activity.contentResolver.openInputStream(data!!.data!!)!!.bufferedReader().readText()
                } catch (e: Exception) {
                    activity.snackbar(e.readableMessage).show()
                }
            }
            else -> super.onActivityResult(requestCode, resultCode, data)
        }
    }

    override fun onDestroy() {
        MainActivity.stateListener = null
        super.onDestroy()
    }
}
