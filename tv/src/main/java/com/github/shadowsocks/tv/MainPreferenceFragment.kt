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
import android.text.format.Formatter
import android.util.Log
import android.widget.Toast
import androidx.fragment.app.FragmentActivity
import androidx.leanback.preference.LeanbackPreferenceFragment
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
import com.github.shadowsocks.ShadowsocksConnection
import com.github.shadowsocks.aidl.IShadowsocksService
import com.github.shadowsocks.aidl.IShadowsocksServiceCallback
import com.github.shadowsocks.bg.BaseService
import com.github.shadowsocks.bg.Executable
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.database.ProfileManager
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.preference.OnPreferenceDataStoreChangeListener
import com.github.shadowsocks.utils.*
import org.json.JSONArray

class MainPreferenceFragment : LeanbackPreferenceFragment(), ShadowsocksConnection.Interface,
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
    override val serviceCallback: IShadowsocksServiceCallback.Stub by lazy {
        object : IShadowsocksServiceCallback.Stub() {
            override fun stateChanged(state: Int, profileName: String?, msg: String?) {
                Core.handler.post { changeState(state, msg) }
            }
            override fun trafficUpdated(profileId: Long, txRate: Long, rxRate: Long, txTotal: Long, rxTotal: Long) {
                stats.summary = getString(R.string.stat_summary,
                        getString(R.string.speed, Formatter.formatFileSize(activity, txRate)),
                        getString(R.string.speed, Formatter.formatFileSize(activity, rxRate)),
                        Formatter.formatFileSize(activity, txTotal), Formatter.formatFileSize(activity, rxTotal))
            }
            override fun trafficPersisted(profileId: Long) { }
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
        val owner = activity as FragmentActivity    // TODO: change to this when refactored to androidx
        if (state != BaseService.CONNECTED) {
            serviceCallback.trafficUpdated(0, 0, 0, 0, 0)
            tester.status.removeObservers(owner)
            if (state != BaseService.IDLE) tester.invalidate()
        } else tester.status.observe(owner, Observer {
            it.retrieve(stats::setTitle) { Toast.makeText(activity, it, Toast.LENGTH_LONG).show() }
        })
        if (msg != null) Toast.makeText(activity, getString(R.string.vpn_error, msg), Toast.LENGTH_SHORT).show()
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

    override val listenForDeath: Boolean get() = true
    override fun onServiceConnected(service: IShadowsocksService) = changeState(try {
        service.state
    } catch (_: DeadObjectException) {
        BaseService.IDLE
    })
    override fun onServiceDisconnected() = changeState(BaseService.IDLE)
    override fun binderDied() {
        super.binderDied()
        Core.handler.post {
            connection.disconnect()
            Executable.killAll()
            connection.connect()
        }
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        preferenceManager.preferenceDataStore = DataStore.publicStore
        DataStore.initGlobal()
        addPreferencesFromResource(R.xml.pref_main)
        fab = findPreference(Key.id) as ListPreference
        populateProfiles()
        stats = findPreference(Key.controlStats)
        controlImport = findPreference(Key.controlImport)

        val boot = findPreference(Key.isAutoConnect) as SwitchPreference
        boot.setOnPreferenceChangeListener { _, value ->
            BootReceiver.enabled = value as Boolean
            true
        }
        boot.isChecked = BootReceiver.enabled

        tfo = findPreference(Key.tfo) as SwitchPreference
        tfo.isChecked = DataStore.tcpFastOpen
        tfo.setOnPreferenceChangeListener { _, value ->
            if (value as Boolean && !TcpFastOpen.sendEnabled) {
                val result = TcpFastOpen.enable()?.trim()
                if (TcpFastOpen.sendEnabled) true else {
                    Toast.makeText(activity, if (result.isNullOrEmpty())
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
        findPreference(Key.about).setOnPreferenceClickListener {
            Toast.makeText(activity, "shadowsocks.org/android", Toast.LENGTH_SHORT).show()
            true
        }

        changeState(BaseService.IDLE)   // reset everything to init state
        connection.connect()
        DataStore.publicStore.registerChangeListener(this)
        tester = ViewModelProviders.of(activity as FragmentActivity).get()
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
            BaseService.usingVpnMode -> {
                val intent = VpnService.prepare(activity)
                if (intent != null) startActivityForResult(intent, REQUEST_CONNECT)
                else onActivityResult(REQUEST_CONNECT, Activity.RESULT_OK, null)
            }
            else -> Core.startService()
        }
    }

    override fun onPreferenceDataStoreChanged(store: PreferenceDataStore, key: String?) {
        when (key) {
            Key.serviceMode -> Core.handler.post {
                connection.disconnect()
                connection.connect()
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
        Toast.makeText(activity, R.string.file_manager_missing, Toast.LENGTH_SHORT).show()
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        when (requestCode) {
            REQUEST_CONNECT -> if (resultCode == Activity.RESULT_OK) Core.startService() else {
                Toast.makeText(activity, R.string.vpn_permission_denied, Toast.LENGTH_SHORT).show()
                Crashlytics.log(Log.ERROR, TAG, "Failed to start VpnService from onActivityResult: $data")
            }
            REQUEST_IMPORT_PROFILES -> {
                if (resultCode != Activity.RESULT_OK) return
                val profiles = ProfileManager.getAllProfiles()?.associateBy { it.formattedAddress }
                val feature = profiles?.values?.singleOrNull { it.id == DataStore.profileId }
                ProfileManager.clear()
                for (uri in data!!.datas) try {
                    Profile.parseJson(activity.contentResolver.openInputStream(uri)!!.bufferedReader().readText(),
                            feature).forEach {
                        // if two profiles has the same address, treat them as the same profile and copy stats over
                        profiles?.get(it.formattedAddress)?.apply {
                            it.tx = tx
                            it.rx = rx
                        }
                        ProfileManager.createProfile(it)
                    }
                } catch (e: Exception) {
                    printLog(e)
                    Toast.makeText(activity, e.localizedMessage, Toast.LENGTH_SHORT).show()
                }
                populateProfiles()
            }
            REQUEST_EXPORT_PROFILES -> {
                if (resultCode != Activity.RESULT_OK) return
                val profiles = ProfileManager.getAllProfiles()
                if (profiles != null) try {
                    activity.contentResolver.openOutputStream(data?.data!!)!!.bufferedWriter().use {
                        it.write(JSONArray(profiles.map { it.toJson() }.toTypedArray()).toString(2))
                    }
                } catch (e: Exception) {
                    printLog(e)
                    Toast.makeText(activity, e.localizedMessage, Toast.LENGTH_SHORT).show()
                }
            }
            else -> super.onActivityResult(requestCode, resultCode, data)
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        DataStore.publicStore.unregisterChangeListener(this)
        connection.disconnect()
        BackupManager(activity).dataChanged()
    }
}
