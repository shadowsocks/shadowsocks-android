/*******************************************************************************
 *                                                                             *
 *  Copyright (C) 2018 by Max Lv <max.c.lv@gmail.com>                          *
 *  Copyright (C) 2018 by Mygod Studio <contact-shadowsocks-android@mygod.be>  *
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

package com.github.shadowsocks.tv

import android.app.Activity
import android.app.backup.BackupManager
import android.content.ActivityNotFoundException
import android.content.Intent
import android.net.VpnService
import android.os.Bundle
import android.os.Handler
import android.os.RemoteException
import android.text.format.Formatter
import android.util.Log
import android.widget.Toast
import androidx.fragment.app.viewModels
import androidx.leanback.preference.LeanbackPreferenceFragmentCompat
import androidx.lifecycle.observe
import androidx.preference.*
import com.crashlytics.android.Crashlytics
import com.github.shadowsocks.BootReceiver
import com.github.shadowsocks.Core
import com.github.shadowsocks.aidl.IShadowsocksService
import com.github.shadowsocks.aidl.ShadowsocksConnection
import com.github.shadowsocks.aidl.TrafficStats
import com.github.shadowsocks.bg.BaseService
import com.github.shadowsocks.database.ProfileManager
import com.github.shadowsocks.net.HttpsTest
import com.github.shadowsocks.net.TcpFastOpen
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.preference.EditTextPreferenceModifiers
import com.github.shadowsocks.preference.HostsSummaryProvider
import com.github.shadowsocks.preference.OnPreferenceDataStoreChangeListener
import com.github.shadowsocks.utils.Key
import com.github.shadowsocks.utils.datas
import com.github.shadowsocks.utils.printLog
import com.github.shadowsocks.utils.readableMessage
import com.google.android.gms.oss.licenses.OssLicensesMenuActivity

class MainPreferenceFragment : LeanbackPreferenceFragmentCompat(), ShadowsocksConnection.Callback,
        OnPreferenceDataStoreChangeListener {
    companion object {
        private const val REQUEST_CONNECT = 1
        private const val REQUEST_REPLACE_PROFILES = 2
        private const val REQUEST_EXPORT_PROFILES = 3
        private const val REQUEST_HOSTS = 4
        private const val TAG = "MainPreferenceFragment"
    }

    private lateinit var fab: ListPreference
    private lateinit var stats: Preference
    private lateinit var controlImport: Preference
    private lateinit var hosts: EditTextPreference
    private lateinit var serviceMode: Preference
    private lateinit var tfo: SwitchPreference
    private lateinit var shareOverLan: Preference
    private lateinit var portProxy: EditTextPreference
    private lateinit var portLocalDns: EditTextPreference
    private lateinit var portTransproxy: EditTextPreference
    private val onServiceModeChange = Preference.OnPreferenceChangeListener { _, newValue ->
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
    private val tester by viewModels<HttpsTest>()

    // service
    var state = BaseService.State.Idle
        private set
    override fun stateChanged(state: BaseService.State, profileName: String?, msg: String?) = changeState(state, msg)
    override fun trafficUpdated(profileId: Long, stats: TrafficStats) {
        if (profileId == 0L) context?.let { context ->
            this.stats.summary = getString(R.string.stat_summary,
                    getString(R.string.speed, Formatter.formatFileSize(context, stats.txRate)),
                    getString(R.string.speed, Formatter.formatFileSize(context, stats.rxRate)),
                    Formatter.formatFileSize(context, stats.txTotal),
                    Formatter.formatFileSize(context, stats.rxTotal))
        }
    }

    private fun changeState(state: BaseService.State, msg: String? = null) {
        val context = context ?: return
        fab.isEnabled = state.canStop || state == BaseService.State.Stopped
        fab.setTitle(when (state) {
            BaseService.State.Connecting -> R.string.connecting
            BaseService.State.Connected -> R.string.stop
            BaseService.State.Stopping -> R.string.stopping
            else -> R.string.connect
        })
        stats.setTitle(R.string.connection_test_pending)
        if ((state == BaseService.State.Connected).also { stats.isVisible = it }) tester.status.observe(this) {
            it.retrieve(stats::setTitle) { msg -> Toast.makeText(context, msg, Toast.LENGTH_LONG).show() }
        } else {
            trafficUpdated(0, TrafficStats())
            tester.status.removeObservers(this)
            if (state != BaseService.State.Idle) tester.invalidate()
        }
        if (msg != null) Toast.makeText(context, getString(R.string.vpn_error, msg), Toast.LENGTH_SHORT).show()
        this.state = state
        val stopped = state == BaseService.State.Stopped
        controlImport.isEnabled = stopped
        tfo.isEnabled = stopped
        serviceMode.isEnabled = stopped
        shareOverLan.isEnabled = stopped
        portProxy.isEnabled = stopped
        if (stopped) onServiceModeChange.onPreferenceChange(null, DataStore.serviceMode) else {
            portLocalDns.isEnabled = false
            portTransproxy.isEnabled = false
        }
    }

    private val handler = Handler()
    private val connection = ShadowsocksConnection(handler, true)
    override fun onServiceConnected(service: IShadowsocksService) = changeState(try {
        BaseService.State.values()[service.state]
    } catch (_: RemoteException) {
        BaseService.State.Idle
    })
    override fun onServiceDisconnected() = changeState(BaseService.State.Idle)
    override fun onBinderDied() {
        connection.disconnect(requireContext())
        connection.connect(requireContext(), this)
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        preferenceManager.preferenceDataStore = DataStore.publicStore
        DataStore.initGlobal()
        addPreferencesFromResource(R.xml.pref_main)
        fab = findPreference(Key.id)!!
        populateProfiles()
        stats = findPreference(Key.controlStats)!!
        controlImport = findPreference(Key.controlImport)!!

        findPreference<SwitchPreference>(Key.persistAcrossReboot)!!.setOnPreferenceChangeListener { _, value ->
            BootReceiver.enabled = value as Boolean
            true
        }

        tfo = findPreference(Key.tfo)!!
        tfo.isChecked = DataStore.tcpFastOpen
        tfo.setOnPreferenceChangeListener { _, value ->
            if (value as Boolean && !TcpFastOpen.sendEnabled) {
                val result = TcpFastOpen.enable()?.trim()
                if (TcpFastOpen.sendEnabled) true else {
                    Toast.makeText(requireContext(), if (result.isNullOrEmpty())
                        getText(R.string.tcp_fastopen_failure) else result, Toast.LENGTH_SHORT).show()
                    false
                }
            } else true
        }
        if (!TcpFastOpen.supported) {
            tfo.isEnabled = false
            tfo.summary = getString(R.string.tcp_fastopen_summary_unsupported, System.getProperty("os.version"))
        }

        hosts = findPreference(Key.hosts)!!
        hosts.summaryProvider = HostsSummaryProvider
        serviceMode = findPreference(Key.serviceMode)!!
        shareOverLan = findPreference(Key.shareOverLan)!!
        portProxy = findPreference(Key.portProxy)!!
        portProxy.setOnBindEditTextListener(EditTextPreferenceModifiers.Port)
        portLocalDns = findPreference(Key.portLocalDns)!!
        portLocalDns.setOnBindEditTextListener(EditTextPreferenceModifiers.Port)
        portTransproxy = findPreference(Key.portTransproxy)!!
        portTransproxy.setOnBindEditTextListener(EditTextPreferenceModifiers.Port)
        serviceMode.onPreferenceChangeListener = onServiceModeChange
        findPreference<Preference>(Key.about)!!.summary = getString(R.string.about_title, BuildConfig.VERSION_NAME)

        changeState(BaseService.State.Idle) // reset everything to init state
        connection.connect(requireContext(), this)
        DataStore.publicStore.registerChangeListener(this)
    }

    override fun onStart() {
        super.onStart()
        connection.bandwidthTimeout = 500
    }

    override fun onResume() {
        super.onResume()
        fab.value = DataStore.profileId.toString()
    }

    private fun populateProfiles() {
        ProfileManager.ensureNotEmpty()
        val profiles = ProfileManager.getActiveProfiles()!!
        fab.value = null
        fab.entries = profiles.map { it.formattedName }.toTypedArray()
        fab.entryValues = profiles.map { it.id.toString() }.toTypedArray()
    }

    fun startService() {
        when {
            state != BaseService.State.Stopped -> return
            DataStore.serviceMode == Key.modeVpn -> {
                val intent = VpnService.prepare(requireContext())
                if (intent != null) startActivityForResult(intent, REQUEST_CONNECT)
                else onActivityResult(REQUEST_CONNECT, Activity.RESULT_OK, null)
            }
            else -> Core.startService()
        }
    }

    override fun onPreferenceDataStoreChanged(store: PreferenceDataStore, key: String) {
        when (key) {
            Key.serviceMode -> handler.post {
                connection.disconnect(requireContext())
                connection.connect(requireContext(), this)
            }
        }
    }

    override fun onStop() {
        connection.bandwidthTimeout = 0
        super.onStop()
    }

    override fun onPreferenceTreeClick(preference: Preference?) = when (preference?.key) {
        Key.id -> {
            if (state == BaseService.State.Connected) Core.stopService()
            true
        }
        Key.controlStats -> {
            tester.testConnection()
            true
        }
        Key.controlImport -> {
            startFilesForResult(Intent(Intent.ACTION_GET_CONTENT).apply {
                type = "application/*"
                putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true)
                putExtra(Intent.EXTRA_MIME_TYPES, arrayOf("application/*", "text/*"))
            }, REQUEST_REPLACE_PROFILES)
            true
        }
        Key.controlExport -> {
            startFilesForResult(Intent(Intent.ACTION_CREATE_DOCUMENT).apply {
                type = "application/json"
                putExtra(Intent.EXTRA_TITLE, "profiles.json")   // optional title that can be edited
            }, REQUEST_EXPORT_PROFILES)
            true
        }
        Key.about -> {
            Toast.makeText(requireContext(), "https://shadowsocks.org/android", Toast.LENGTH_SHORT).show()
            true
        }
        Key.aboutOss -> {
            startActivity(Intent(context, OssLicensesMenuActivity::class.java))
            true
        }
        else -> super.onPreferenceTreeClick(preference)
    }

    override fun onDisplayPreferenceDialog(preference: Preference?) {
        if (preference != hosts || startFilesForResult(Intent(Intent.ACTION_GET_CONTENT).setType("*/*"), REQUEST_HOSTS))
            super.onDisplayPreferenceDialog(preference)
    }

    private fun startFilesForResult(intent: Intent, requestCode: Int): Boolean {
        try {
            startActivityForResult(intent.addCategory(Intent.CATEGORY_OPENABLE), requestCode)
            return false
        } catch (_: ActivityNotFoundException) { } catch (_: SecurityException) { }
        Toast.makeText(requireContext(), R.string.file_manager_missing, Toast.LENGTH_SHORT).show()
        return true
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        when (requestCode) {
            REQUEST_CONNECT -> if (resultCode == Activity.RESULT_OK) Core.startService() else {
                Toast.makeText(requireContext(), R.string.vpn_permission_denied, Toast.LENGTH_SHORT).show()
                Crashlytics.log(Log.ERROR, TAG, "Failed to start VpnService from onActivityResult: $data")
            }
            REQUEST_REPLACE_PROFILES -> {
                if (resultCode != Activity.RESULT_OK) return
                val context = requireContext()
                try {
                    ProfileManager.createProfilesFromJson(data!!.datas.asSequence().map {
                        context.contentResolver.openInputStream(it)
                    }.filterNotNull(), true)
                } catch (e: Exception) {
                    printLog(e)
                    Toast.makeText(context, e.readableMessage, Toast.LENGTH_SHORT).show()
                }
                populateProfiles()
            }
            REQUEST_EXPORT_PROFILES -> {
                if (resultCode != Activity.RESULT_OK) return
                val profiles = ProfileManager.serializeToJson()
                val context = requireContext()
                if (profiles != null) try {
                    context.contentResolver.openOutputStream(data?.data!!)!!.bufferedWriter().use {
                        it.write(profiles.toString(2))
                    }
                } catch (e: Exception) {
                    printLog(e)
                    Toast.makeText(context, e.readableMessage, Toast.LENGTH_SHORT).show()
                }
            }
            REQUEST_HOSTS -> {
                if (resultCode != Activity.RESULT_OK) return
                val context = requireContext()
                try {
                    // we read and persist all its content here to avoid content URL permission issues
                    hosts.text = context.contentResolver.openInputStream(data!!.data!!)!!.bufferedReader().readText()
                } catch (e: Exception) {
                    Toast.makeText(context, e.readableMessage, Toast.LENGTH_SHORT).show()
                }
            }
            else -> super.onActivityResult(requestCode, resultCode, data)
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        DataStore.publicStore.unregisterChangeListener(this)
        val context = requireContext()
        connection.disconnect(context)
        BackupManager(context).dataChanged()
    }
}
