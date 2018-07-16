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

import android.os.Build
import android.os.Bundle
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.LinearLayout
import androidx.appcompat.widget.Toolbar
import com.google.android.material.snackbar.Snackbar
import androidx.preference.SwitchPreference
import androidx.preference.Preference
import com.github.shadowsocks.App.Companion.app
import com.github.shadowsocks.bg.BaseService
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.utils.DirectBoot
import com.github.shadowsocks.utils.Key
import com.github.shadowsocks.utils.TcpFastOpen
import com.takisoft.preferencex.PreferenceFragmentCompat

class GlobalSettingsPreferenceFragment : PreferenceFragmentCompat() {
    private lateinit var toolbar: Toolbar
    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        toolbar = inflater.inflate(R.layout.toolbar_light_dark, container, false) as Toolbar
        return super.onCreateView(inflater, container, savedInstanceState)
    }
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        val child = (view as LinearLayout).getChildAt(0)
        view.removeAllViews()
        view.orientation = LinearLayout.VERTICAL
        view.addView(toolbar, 0)
        view.addView(child, 1)
        super.onViewCreated(view, savedInstanceState)
        toolbar.setTitle(R.string.settings)
        toolbar.setNavigationIcon(R.drawable.ic_navigation_menu)
        toolbar.setNavigationOnClickListener { (activity as MainActivity).drawer.openDrawer(Gravity.START) }
    }
    override fun onCreatePreferencesFix(savedInstanceState: Bundle?, rootKey: String?) {
        preferenceManager.preferenceDataStore = DataStore.publicStore
        addPreferencesFromResource(R.xml.pref_global)
        val boot = findPreference(Key.isAutoConnect) as SwitchPreference
        boot.setOnPreferenceChangeListener { _, value ->
            BootReceiver.enabled = value as Boolean
            true
        }
        boot.isChecked = BootReceiver.enabled

        val canToggleLocked = findPreference(Key.directBootAware)
        if (Build.VERSION.SDK_INT >= 24) canToggleLocked.setOnPreferenceChangeListener { _, newValue ->
            if (app.directBootSupported && newValue as Boolean) DirectBoot.update() else DirectBoot.clean()
            true
        } else canToggleLocked.parent!!.removePreference(canToggleLocked)

        val tfo = findPreference(Key.tfo) as SwitchPreference
        tfo.isChecked = TcpFastOpen.sendEnabled
        tfo.setOnPreferenceChangeListener { _, value ->
            val result = TcpFastOpen.enabled(value as Boolean)
            if (result != null && result != "Success.")
                Snackbar.make(requireActivity().findViewById(R.id.snackbar), result, Snackbar.LENGTH_LONG).show()
            value == TcpFastOpen.sendEnabled
        }
        if (!TcpFastOpen.supported) {
            tfo.isEnabled = false
            tfo.summary = getString(R.string.tcp_fastopen_summary_unsupported, System.getProperty("os.version"))
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
            portLocalDns.isEnabled = enabledLocalDns
            portTransproxy.isEnabled = enabledTransproxy
            true
        }
        val listener: (Int) -> Unit = {
            when (it) {
                BaseService.IDLE, BaseService.STOPPED -> {
                    serviceMode.isEnabled = true
                    portProxy.isEnabled = true
                    onServiceModeChange.onPreferenceChange(null, DataStore.serviceMode)
                }
                else -> {
                    serviceMode.isEnabled = false
                    portProxy.isEnabled = false
                    portLocalDns.isEnabled = false
                    portTransproxy.isEnabled = false
                }
            }
        }
        listener((activity as MainActivity).state)
        MainActivity.stateListener = listener
        serviceMode.onPreferenceChangeListener = onServiceModeChange
    }

    override fun onDestroy() {
        MainActivity.stateListener = null
        super.onDestroy()
    }
}
