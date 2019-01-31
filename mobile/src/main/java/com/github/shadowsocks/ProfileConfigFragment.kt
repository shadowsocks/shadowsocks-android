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
import android.content.BroadcastReceiver
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.os.UserManager
import android.view.MenuItem
import androidx.appcompat.app.AlertDialog
import androidx.core.content.getSystemService
import androidx.core.os.bundleOf
import androidx.preference.Preference
import androidx.preference.PreferenceDataStore
import androidx.preference.SwitchPreference
import com.github.shadowsocks.Core.app
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.database.ProfileManager
import com.github.shadowsocks.plugin.PluginConfiguration
import com.github.shadowsocks.plugin.PluginContract
import com.github.shadowsocks.plugin.PluginManager
import com.github.shadowsocks.plugin.PluginOptions
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.preference.IconListPreference
import com.github.shadowsocks.preference.OnPreferenceDataStoreChangeListener
import com.github.shadowsocks.preference.PluginConfigurationDialogFragment
import com.github.shadowsocks.utils.Action
import com.github.shadowsocks.utils.DirectBoot
import com.github.shadowsocks.utils.Key
import com.google.android.material.snackbar.Snackbar
import com.takisoft.preferencex.EditTextPreference
import com.takisoft.preferencex.PreferenceFragmentCompat

class ProfileConfigFragment : PreferenceFragmentCompat(),
        Preference.OnPreferenceChangeListener, OnPreferenceDataStoreChangeListener {
    companion object {
        private const val REQUEST_CODE_PLUGIN_CONFIGURE = 1
    }

    private var profileId = -1L
    private lateinit var isProxyApps: SwitchPreference
    private lateinit var plugin: IconListPreference
    private lateinit var pluginConfigure: EditTextPreference
    private lateinit var pluginConfiguration: PluginConfiguration
    private lateinit var receiver: BroadcastReceiver
    private lateinit var udpFallback: Preference

    override fun onCreatePreferencesFix(savedInstanceState: Bundle?, rootKey: String?) {
        preferenceManager.preferenceDataStore = DataStore.privateStore
        val activity = requireActivity()
        profileId = activity.intent.getLongExtra(Action.EXTRA_PROFILE_ID, -1L)
        addPreferencesFromResource(R.xml.pref_profile)
        if (Build.VERSION.SDK_INT >= 25 && activity.getSystemService<UserManager>()?.isDemoUser == true) {
            findPreference(Key.host).summary = "shadowsocks.example.org"
            findPreference(Key.remotePort).summary = "1337"
            findPreference(Key.password).summary = "\u2022".repeat(32)
        }
        val serviceMode = DataStore.serviceMode
        findPreference(Key.remoteDns).isEnabled = serviceMode != Key.modeProxy
        isProxyApps = findPreference(Key.proxyApps) as SwitchPreference
        isProxyApps.isEnabled = serviceMode == Key.modeVpn
        isProxyApps.setOnPreferenceClickListener {
            startActivity(Intent(activity, AppManager::class.java))
            isProxyApps.isChecked = true
            false
        }
        findPreference(Key.udpdns).isEnabled = serviceMode != Key.modeProxy
        plugin = findPreference(Key.plugin) as IconListPreference
        pluginConfigure = findPreference(Key.pluginConfigure) as EditTextPreference
        plugin.unknownValueSummary = getString(R.string.plugin_unknown)
        plugin.setOnPreferenceChangeListener { _, newValue ->
            pluginConfiguration = PluginConfiguration(pluginConfiguration.pluginsOptions, newValue as String)
            DataStore.plugin = pluginConfiguration.toString()
            DataStore.dirty = true
            pluginConfigure.isEnabled = newValue.isNotEmpty()
            pluginConfigure.text = pluginConfiguration.selectedOptions.toString()
            if (PluginManager.fetchPlugins()[newValue]?.trusted == false) {
                Snackbar.make(view!!, R.string.plugin_untrusted, Snackbar.LENGTH_LONG).show()
            }
            true
        }
        pluginConfigure.onPreferenceChangeListener = this
        initPlugins()
        receiver = Core.listenForPackageChanges(false) { initPlugins() }
        udpFallback = findPreference(Key.udpFallback)
        DataStore.privateStore.registerChangeListener(this)
    }

    private fun initPlugins() {
        val plugins = PluginManager.fetchPlugins()
        plugin.entries = plugins.map { it.value.label }.toTypedArray()
        plugin.entryValues = plugins.map { it.value.id }.toTypedArray()
        plugin.entryIcons = plugins.map { it.value.icon }.toTypedArray()
        plugin.entryPackageNames = plugins.map { it.value.packageName }.toTypedArray()
        pluginConfiguration = PluginConfiguration(DataStore.plugin)
        plugin.value = pluginConfiguration.selected
        plugin.init()
        plugin.checkSummary()
        pluginConfigure.isEnabled = pluginConfiguration.selected.isNotEmpty()
        pluginConfigure.text = pluginConfiguration.selectedOptions.toString()
    }

    private fun showPluginEditor() = displayPreferenceDialog(PluginConfigurationDialogFragment(), Key.pluginConfigure,
            bundleOf(Pair("key", Key.pluginConfigure),
                    Pair(PluginConfigurationDialogFragment.PLUGIN_ID_FRAGMENT_TAG, pluginConfiguration.selected)))

    fun saveAndExit() {
        val profile = ProfileManager.getProfile(profileId) ?: Profile()
        profile.id = profileId
        profile.deserialize()
        ProfileManager.updateProfile(profile)
        ProfilesFragment.instance?.profilesAdapter?.deepRefreshId(profileId)
        if (profileId in Core.activeProfileIds && DataStore.directBootAware) DirectBoot.update()
        requireActivity().finish()
    }

    override fun onResume() {
        super.onResume()
        isProxyApps.isChecked = DataStore.proxyApps // fetch proxyApps updated by AppManager
        val fallbackProfile = DataStore.udpFallback?.let { ProfileManager.getProfile(it) }
        if (fallbackProfile == null) udpFallback.setSummary(R.string.plugin_disabled)
        else udpFallback.summary = fallbackProfile.formattedName
    }

    override fun onPreferenceChange(preference: Preference?, newValue: Any?): Boolean = try {
        val selected = pluginConfiguration.selected
        pluginConfiguration = PluginConfiguration(pluginConfiguration.pluginsOptions +
                (pluginConfiguration.selected to PluginOptions(selected, newValue as? String?)), selected)
        DataStore.plugin = pluginConfiguration.toString()
        DataStore.dirty = true
        true
    } catch (exc: RuntimeException) {
        Snackbar.make(view!!, exc.localizedMessage, Snackbar.LENGTH_LONG).show()
        false
    }

    override fun onPreferenceDataStoreChanged(store: PreferenceDataStore, key: String?) {
        if (key != Key.proxyApps && findPreference(key) != null) DataStore.dirty = true
    }

    override fun onDisplayPreferenceDialog(preference: Preference) {
        if (preference.key == Key.pluginConfigure) {
            val intent = PluginManager.buildIntent(pluginConfiguration.selected, PluginContract.ACTION_CONFIGURE)
            if (intent.resolveActivity(requireContext().packageManager) == null) showPluginEditor() else
                startActivityForResult(intent
                        .putExtra(PluginContract.EXTRA_OPTIONS, pluginConfiguration.selectedOptions.toString()),
                        REQUEST_CODE_PLUGIN_CONFIGURE)
        } else super.onDisplayPreferenceDialog(preference)
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        if (requestCode == REQUEST_CODE_PLUGIN_CONFIGURE) when (resultCode) {
            Activity.RESULT_OK -> {
                val options = data?.getStringExtra(PluginContract.EXTRA_OPTIONS)
                pluginConfigure.text = options
                onPreferenceChange(null, options)
            }
            PluginContract.RESULT_FALLBACK -> showPluginEditor()
        } else super.onActivityResult(requestCode, resultCode, data)
    }

    override fun onOptionsItemSelected(item: MenuItem) = when (item.itemId) {
        R.id.action_delete -> {
            val activity = requireActivity()
            AlertDialog.Builder(activity)
                    .setTitle(R.string.delete_confirm_prompt)
                    .setPositiveButton(R.string.yes) { _, _ ->
                        ProfileManager.delProfile(profileId)
                        activity.finish()
                    }
                    .setNegativeButton(R.string.no, null)
                    .create()
                    .show()
            true
        }
        R.id.action_apply -> {
            saveAndExit()
            true
        }
        else -> false
    }

    override fun onDestroy() {
        DataStore.privateStore.unregisterChangeListener(this)
        app.unregisterReceiver(receiver)
        super.onDestroy()
    }
}
