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
import android.os.DeadObjectException
import android.os.Handler
import android.text.format.Formatter
import android.util.Log
import android.util.LongSparseArray
import android.widget.Toast
import androidx.leanback.preference.LeanbackPreferenceFragmentCompat
import androidx.lifecycle.Observer
import androidx.lifecycle.ViewModelProviders
import androidx.lifecycle.get
import androidx.preference.ListPreference
import androidx.preference.Preference
import androidx.preference.PreferenceDataStore
import androidx.preference.SwitchPreference
import com.crashlytics.android.Crashlytics
import com.github.shadowsocks.BootReceiver
import com.github.shadowsocks.Core
import com.github.shadowsocks.aidl.IShadowsocksService
import com.github.shadowsocks.aidl.ShadowsocksConnection
import com.github.shadowsocks.aidl.TrafficStats
import com.github.shadowsocks.bg.BaseService
import com.github.shadowsocks.bg.Executable
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.database.ProfileManager
import com.github.shadowsocks.net.HttpsTest
import com.github.shadowsocks.net.TcpFastOpen
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.preference.OnPreferenceDataStoreChangeListener
import com.github.shadowsocks.utils.Key
import com.github.shadowsocks.utils.datas
import com.github.shadowsocks.utils.printLog
import org.json.JSONArray

class MainPreferenceFragment : LeanbackPreferenceFragmentCompat(), ShadowsocksConnection.Callback,
        OnPreferenceDataStoreChangeListener {
    companion object {
        private const val REQUEST_CONNECT = 1
        private const val REQUEST_IMPORT_PROFILES = 2
        private const val REQUEST_EXPORT_PROFILES = 3
        private const val TAG = "MainPreferenceFragment"
    }

    private lateinit var fab: ListPreference
    private lateinit var stats: Preference
    private lateinit var controlImport: Preference
    private lateinit var serviceMode: Preference
    private lateinit var tfo: SwitchPreference
    private lateinit var shareOverLan: Preference
    private lateinit var portProxy: Preference
    private lateinit var portLocalDns: Preference
    private lateinit var portTransproxy: Preference
    private val onServiceModeChange = Preference.OnPreferenceChangeListener { _, newValue ->
        val (enabledLocalDns, enabledTransproxy) = when (newValue as String?) {
            Key.modeProxy -> Pair(false, false)
            Key.modeVpn -> Pair(true, false)
            Key.modeTransproxy -> Pair(true, true)
            else -> throw IllegalArgumentException("newValue: $newValue")
        }
        portLocalDns.isEnabled = enabledLocalDns
        portTransproxy.isEnabled = enabledTransproxy
        true
    }
    private lateinit var tester: HttpsTest

    // service
    var state = BaseService.IDLE
        private set
    override fun stateChanged(state: Int, profileName: String?, msg: String?) = changeState(state, msg)
    override fun trafficUpdated(profileId: Long, stats: TrafficStats) {
        if (profileId == 0L) requireContext().let { context ->
            this.stats.summary = getString(R.string.stat_summary,
                    getString(R.string.speed, Formatter.formatFileSize(context, stats.txRate)),
                    getString(R.string.speed, Formatter.formatFileSize(context, stats.rxRate)),
                    Formatter.formatFileSize(context, stats.txTotal),
                    Formatter.formatFileSize(context, stats.rxTotal))
        }
    }

    private fun changeState(state: Int, msg: String? = null) {
        fab.isEnabled = state == BaseService.STOPPED || state == BaseService.CONNECTED
        fab.setTitle(when (state) {
            BaseService.CONNECTING -> R.string.connecting
            BaseService.CONNECTED -> R.string.stop
            BaseService.STOPPING -> R.string.stopping
            else -> R.string.connect
        })
        stats.setTitle(R.string.connection_test_pending)
        stats.isVisible = state == BaseService.CONNECTED
        if (state != BaseService.CONNECTED) {
            trafficUpdated(0, TrafficStats())
            tester.status.removeObservers(this)
            if (state != BaseService.IDLE) tester.invalidate()
        } else tester.status.observe(this, Observer {
            it.retrieve(stats::setTitle) { Toast.makeText(requireContext(), it, Toast.LENGTH_LONG).show() }
        })
        if (msg != null) Toast.makeText(requireContext(), getString(R.string.vpn_error, msg), Toast.LENGTH_SHORT).show()
        this.state = state
        if (state == BaseService.STOPPED) {
            controlImport.isEnabled = true
            tfo.isEnabled = true
            serviceMode.isEnabled = true
            shareOverLan.isEnabled = true
            portProxy.isEnabled = true
            onServiceModeChange.onPreferenceChange(null, DataStore.serviceMode)
        } else {
            controlImport.isEnabled = false
            tfo.isEnabled = false
            serviceMode.isEnabled = false
            shareOverLan.isEnabled = false
            portProxy.isEnabled = false
            portLocalDns.isEnabled = false
            portTransproxy.isEnabled = false
        }
    }

    private val handler = Handler()
    private val connection = ShadowsocksConnection(handler, true)
    override fun onServiceConnected(service: IShadowsocksService) = changeState(try {
        service.state
    } catch (_: DeadObjectException) {
        BaseService.IDLE
    })
    override fun onServiceDisconnected() = changeState(BaseService.IDLE)
    override fun onBinderDied() {
        connection.disconnect(requireContext())
        Executable.killAll()
        connection.connect(requireContext(), this)
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        preferenceManager.preferenceDataStore = DataStore.publicStore
        DataStore.initGlobal()
        addPreferencesFromResource(R.xml.pref_main)
        fab = findPreference(Key.id) as ListPreference
        populateProfiles()
        stats = findPreference(Key.controlStats)
        controlImport = findPreference(Key.controlImport)

        (findPreference(Key.isAutoConnect) as SwitchPreference).apply {
            setOnPreferenceChangeListener { _, value ->
                BootReceiver.enabled = value as Boolean
                true
            }
            isChecked = BootReceiver.enabled
        }

        tfo = findPreference(Key.tfo) as SwitchPreference
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

        serviceMode = findPreference(Key.serviceMode)
        shareOverLan = findPreference(Key.shareOverLan)
        portProxy = findPreference(Key.portProxy)
        portLocalDns = findPreference(Key.portLocalDns)
        portTransproxy = findPreference(Key.portTransproxy)
        serviceMode.onPreferenceChangeListener = onServiceModeChange
        findPreference(Key.about).apply {
            summary = getString(R.string.about_title, BuildConfig.VERSION_NAME)
            setOnPreferenceClickListener {
                Toast.makeText(requireContext(), "https://shadowsocks.org/android", Toast.LENGTH_SHORT).show()
                true
            }
        }

        tester = ViewModelProviders.of(this).get()
        changeState(BaseService.IDLE)   // reset everything to init state
        connection.connect(requireContext(), this)
        DataStore.publicStore.registerChangeListener(this)
    }

    override fun onStart() {
        super.onStart()
        connection.listeningForBandwidth = true
    }

    override fun onResume() {
        super.onResume()
        fab.value = DataStore.profileId.toString()
    }

    private fun populateProfiles() {
        ProfileManager.ensureNotEmpty()
        val profiles = ProfileManager.getAllProfiles()!!
        fab.value = null
        fab.entries = profiles.map { it.formattedName }.toTypedArray()
        fab.entryValues = profiles.map { it.id.toString() }.toTypedArray()
    }

    fun startService() {
        when {
            state != BaseService.STOPPED -> return
            DataStore.serviceMode == Key.modeVpn -> {
                val intent = VpnService.prepare(requireContext())
                if (intent != null) startActivityForResult(intent, REQUEST_CONNECT)
                else onActivityResult(REQUEST_CONNECT, Activity.RESULT_OK, null)
            }
            else -> Core.startService()
        }
    }

    override fun onPreferenceDataStoreChanged(store: PreferenceDataStore, key: String?) {
        when (key) {
            Key.serviceMode -> handler.post {
                connection.disconnect(requireContext())
                connection.connect(requireContext(), this)
            }
        }
    }

    override fun onStop() {
        connection.listeningForBandwidth = false
        super.onStop()
    }

    override fun onPreferenceTreeClick(preference: Preference?) = when (preference?.key) {
        Key.id -> {
            if (state == BaseService.CONNECTED) Core.stopService()
            true
        }
        Key.controlStats -> {
            tester.testConnection()
            true
        }
        Key.controlImport -> {
            startFilesForResult(Intent(Intent.ACTION_GET_CONTENT).apply {
                addCategory(Intent.CATEGORY_OPENABLE)
                type = "application/json"
                putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true)
            }, REQUEST_IMPORT_PROFILES)
            true
        }
        Key.controlExport -> {
            startFilesForResult(Intent(Intent.ACTION_CREATE_DOCUMENT).apply {
                addCategory(Intent.CATEGORY_OPENABLE)
                type = "application/json"
                putExtra(Intent.EXTRA_TITLE, "profiles.json")   // optional title that can be edited
            }, REQUEST_EXPORT_PROFILES)
            true
        }
        else -> super.onPreferenceTreeClick(preference)
    }

    private fun startFilesForResult(intent: Intent?, requestCode: Int) {
        try {
            startActivityForResult(intent, requestCode)
            return
        } catch (_: ActivityNotFoundException) { } catch (_: SecurityException) { }
        Toast.makeText(requireContext(), R.string.file_manager_missing, Toast.LENGTH_SHORT).show()
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        when (requestCode) {
            REQUEST_CONNECT -> if (resultCode == Activity.RESULT_OK) Core.startService() else {
                Toast.makeText(requireContext(), R.string.vpn_permission_denied, Toast.LENGTH_SHORT).show()
                Crashlytics.log(Log.ERROR, TAG, "Failed to start VpnService from onActivityResult: $data")
            }
            REQUEST_IMPORT_PROFILES -> {
                if (resultCode != Activity.RESULT_OK) return
                val profiles = ProfileManager.getAllProfiles()?.associateBy { it.formattedAddress }
                val feature = profiles?.values?.singleOrNull { it.id == DataStore.profileId }
                val lazyClear = lazy { ProfileManager.clear() }
                val context = requireContext()
                for (uri in data!!.datas) try {
                    Profile.parseJson(context.contentResolver.openInputStream(uri)!!.bufferedReader().readText(),
                            feature) {
                        lazyClear.value
                        // if two profiles has the same address, treat them as the same profile and copy stats over
                        profiles?.get(it.formattedAddress)?.apply {
                            it.tx = tx
                            it.rx = rx
                        }
                        ProfileManager.createProfile(it)
                    }
                } catch (e: Exception) {
                    printLog(e)
                    Toast.makeText(context, e.localizedMessage, Toast.LENGTH_SHORT).show()
                }
                populateProfiles()
            }
            REQUEST_EXPORT_PROFILES -> {
                if (resultCode != Activity.RESULT_OK) return
                val profiles = ProfileManager.getAllProfiles()
                val context = requireContext()
                if (profiles != null) try {
                    val lookup = LongSparseArray<Profile>(profiles.size).apply { profiles.forEach { put(it.id, it) } }
                    context.contentResolver.openOutputStream(data?.data!!)!!.bufferedWriter().use {
                        it.write(JSONArray(profiles.map { it.toJson(lookup) }.toTypedArray()).toString(2))
                    }
                } catch (e: Exception) {
                    printLog(e)
                    Toast.makeText(context, e.localizedMessage, Toast.LENGTH_SHORT).show()
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
