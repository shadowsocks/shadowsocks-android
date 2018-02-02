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
import android.os.UserManager
import android.support.design.widget.Snackbar
import android.support.v14.preference.SwitchPreference
import android.support.v7.app.AlertDialog
import android.support.v7.preference.Preference
import android.support.v7.preference.PreferenceDataStore
import android.support.v7.widget.Toolbar
import android.view.MenuItem
import com.github.shadowsocks.App.Companion.app
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
import com.takisoft.fix.support.v7.preference.EditTextPreference
import com.takisoft.fix.support.v7.preference.PreferenceFragmentCompatDividers

class ProfileConfigFragment : PreferenceFragmentCompatDividers(), Toolbar.OnMenuItemClickListener,
        Preference.OnPreferenceChangeListener, OnPreferenceDataStoreChangeListener {
    companion object {
        private const val REQUEST_CODE_PLUGIN_CONFIGURE = 1
    }

    private lateinit var profile: Profile
    private lateinit var isProxyApps: SwitchPreference
    private lateinit var plugin: IconListPreference
    private lateinit var pluginConfigure: EditTextPreference
    private lateinit var pluginConfiguration: PluginConfiguration

    override fun onCreatePreferencesFix(savedInstanceState: Bundle?, rootKey: String?) {
        preferenceManager.preferenceDataStore = DataStore.privateStore
        val activity = activity!!
        val profile = ProfileManager.getProfile(activity.intent.getIntExtra(Action.EXTRA_PROFILE_ID, -1))
        if (profile == null) {
            activity.finish()
            return
        }
        this.profile = profile
        profile.serialize()
        addPreferencesFromResource(R.xml.pref_profile)
        if (Build.VERSION.SDK_INT >= 25 && activity.getSystemService(UserManager::class.java).isDemoUser) {
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
            if (PluginManager.fetchPlugins()[newValue]?.trusted == false)
                Snackbar.make(view!!, R.string.plugin_untrusted, Snackbar.LENGTH_LONG).show()
            true
        }
        pluginConfigure.onPreferenceChangeListener = this
        initPlugins()
        app.listenForPackageChanges { initPlugins() }
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

    private fun showPluginEditor() {
        val bundle = Bundle()
        bundle.putString("key", Key.pluginConfigure)
        bundle.putString(PluginConfigurationDialogFragment.PLUGIN_ID_FRAGMENT_TAG, pluginConfiguration.selected)
        displayPreferenceDialog(PluginConfigurationDialogFragment(), Key.pluginConfigure, bundle)
    }

    fun saveAndExit() {
        profile.deserialize()
        ProfileManager.updateProfile(profile)
        ProfilesFragment.instance?.profilesAdapter?.deepRefreshId(profile.id)
        if (DataStore.profileId == profile.id && DataStore.directBootAware) DirectBoot.update()
        activity!!.finish()
    }

    override fun onResume() {
        super.onResume()
        isProxyApps.isChecked = DataStore.proxyApps // fetch proxyApps updated by AppManager
    }

    override fun onPreferenceChange(preference: Preference?, newValue: Any?): Boolean = try {
        val selected = pluginConfiguration.selected
        pluginConfiguration = PluginConfiguration(pluginConfiguration.pluginsOptions +
                (pluginConfiguration.selected to PluginOptions(selected, newValue as? String?)), selected)
        DataStore.plugin = pluginConfiguration.toString()
        DataStore.dirty = true
        true
    } catch (exc: IllegalArgumentException) {
        Snackbar.make(view!!, exc.localizedMessage, Snackbar.LENGTH_LONG).show()
        false
    }

    override fun onPreferenceDataStoreChanged(store: PreferenceDataStore, key: String?) {
        if (key != Key.proxyApps && findPreference(key) != null) DataStore.dirty = true
    }

    override fun onDisplayPreferenceDialog(preference: Preference) {
        if (preference.key == Key.pluginConfigure) {
            val intent = PluginManager.buildIntent(pluginConfiguration.selected, PluginContract.ACTION_CONFIGURE)
            if (intent.resolveActivity(activity!!.packageManager) != null)
                startActivityForResult(intent.putExtra(PluginContract.EXTRA_OPTIONS,
                        pluginConfiguration.selectedOptions.toString()), REQUEST_CODE_PLUGIN_CONFIGURE) else {
                showPluginEditor()
            }
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

    override fun onMenuItemClick(item: MenuItem): Boolean {
        return when (item.itemId) {
            R.id.action_delete -> {
                val activity = activity!!
                AlertDialog.Builder(activity)
                        .setTitle(R.string.delete_confirm_prompt)
                        .setPositiveButton(R.string.yes, { _, _ ->
                            ProfileManager.delProfile(profile.id)
                            activity.finish()
                        })
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
    }

    override fun onDestroy() {
        DataStore.privateStore.unregisterChangeListener(this)
        super.onDestroy()
    }
}
