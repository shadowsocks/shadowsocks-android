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
import android.content.DialogInterface
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.os.Parcelable
import android.view.MenuItem
import android.view.View
import androidx.appcompat.app.AlertDialog
import androidx.core.view.updatePadding
import androidx.preference.*
import com.github.shadowsocks.Core.app
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.database.ProfileManager
import com.github.shadowsocks.plugin.*
import com.github.shadowsocks.preference.*
import com.github.shadowsocks.utils.*
import com.google.android.material.snackbar.Snackbar
import kotlinx.android.parcel.Parcelize

class ProfileConfigFragment : PreferenceFragmentCompat(),
        Preference.OnPreferenceChangeListener, OnPreferenceDataStoreChangeListener {
    companion object PasswordSummaryProvider : Preference.SummaryProvider<EditTextPreference> {
        override fun provideSummary(preference: EditTextPreference?) = "\u2022".repeat(preference?.text?.length ?: 0)

        private const val REQUEST_CODE_PLUGIN_CONFIGURE = 1
        const val REQUEST_UNSAVED_CHANGES = 2
    }

    @Parcelize
    data class ProfileIdArg(val profileId: Long) : Parcelable
    class DeleteConfirmationDialogFragment : AlertDialogFragment<ProfileIdArg, Empty>() {
        override fun AlertDialog.Builder.prepare(listener: DialogInterface.OnClickListener) {
            setTitle(R.string.delete_confirm_prompt)
            setPositiveButton(R.string.yes) { _, _ ->
                ProfileManager.delProfile(arg.profileId)
                requireActivity().finish()
            }
            setNegativeButton(R.string.no, null)
        }
    }

    private var profileId = -1L
    private lateinit var isProxyApps: SwitchPreference
    private lateinit var plugin: IconListPreference
    private lateinit var pluginConfigure: EditTextPreference
    private lateinit var pluginConfiguration: PluginConfiguration
    private lateinit var receiver: BroadcastReceiver
    private lateinit var udpFallback: Preference

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        preferenceManager.preferenceDataStore = DataStore.privateStore
        val activity = requireActivity()
        profileId = activity.intent.getLongExtra(Action.EXTRA_PROFILE_ID, -1L)
        addPreferencesFromResource(R.xml.pref_profile)
        findPreference<EditTextPreference>(Key.remotePort)!!.setOnBindEditTextListener(EditTextPreferenceModifiers.Port)
        findPreference<EditTextPreference>(Key.password)!!.summaryProvider = PasswordSummaryProvider
        val serviceMode = DataStore.serviceMode
        findPreference<Preference>(Key.remoteDns)!!.isEnabled = serviceMode != Key.modeProxy
        findPreference<Preference>(Key.ipv6)!!.isEnabled = serviceMode == Key.modeVpn
        isProxyApps = findPreference(Key.proxyApps)!!
        isProxyApps.isEnabled = serviceMode == Key.modeVpn
        isProxyApps.setOnPreferenceClickListener {
            startActivity(Intent(activity, AppManager::class.java))
            isProxyApps.isChecked = true
            false
        }
        findPreference<Preference>(Key.metered)!!.apply {
            if (Build.VERSION.SDK_INT >= 28) isEnabled = serviceMode == Key.modeVpn else remove()
        }
        findPreference<Preference>(Key.udpdns)!!.isEnabled = serviceMode != Key.modeProxy
        plugin = findPreference(Key.plugin)!!
        pluginConfigure = findPreference(Key.pluginConfigure)!!
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
        pluginConfigure.setOnBindEditTextListener(EditTextPreferenceModifiers.Monospace)
        pluginConfigure.onPreferenceChangeListener = this
        initPlugins()
        receiver = Core.listenForPackageChanges(false) { initPlugins() }
        udpFallback = findPreference(Key.udpFallback)!!
        DataStore.privateStore.registerChangeListener(this)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        listView.setOnApplyWindowInsetsListener { v, insets ->
            v.updatePadding(bottom = v.paddingBottom + insets.systemWindowInsetBottom)
            insets.consumeSystemWindowInsets()
        }
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
        pluginConfigure.isEnabled = pluginConfiguration.selected.isNotEmpty()
        pluginConfigure.text = pluginConfiguration.selectedOptions.toString()
    }

    private fun showPluginEditor() {
        PluginConfigurationDialogFragment().apply {
            setArg(Key.pluginConfigure, pluginConfiguration.selected)
            setTargetFragment(this@ProfileConfigFragment, 0)
        }.show(fragmentManager ?: return, Key.pluginConfigure)
    }

    private fun saveAndExit() {
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
        Snackbar.make(view!!, exc.readableMessage, Snackbar.LENGTH_LONG).show()
        false
    }

    override fun onPreferenceDataStoreChanged(store: PreferenceDataStore, key: String) {
        if (key != Key.proxyApps && findPreference<Preference>(key) != null) DataStore.dirty = true
    }

    override fun onDisplayPreferenceDialog(preference: Preference) {
        when (preference.key) {
            Key.plugin -> BottomSheetPreferenceDialogFragment().apply {
                setArg(Key.plugin)
                setTargetFragment(this@ProfileConfigFragment, 0)
            }.show(fragmentManager ?: return, Key.plugin)
            Key.pluginConfigure -> {
                val intent = PluginManager.buildIntent(pluginConfiguration.selected, PluginContract.ACTION_CONFIGURE)
                if (intent.resolveActivity(requireContext().packageManager) == null) showPluginEditor() else {
                    startActivityForResult(intent
                            .putExtra(PluginContract.EXTRA_OPTIONS, pluginConfiguration.selectedOptions.toString()),
                            REQUEST_CODE_PLUGIN_CONFIGURE)
                }
            }
            else -> super.onDisplayPreferenceDialog(preference)
        }
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        when (requestCode) {
            REQUEST_CODE_PLUGIN_CONFIGURE -> when (resultCode) {
                Activity.RESULT_OK -> {
                    val options = data?.getStringExtra(PluginContract.EXTRA_OPTIONS)
                    pluginConfigure.text = options
                    onPreferenceChange(null, options)
                }
                PluginContract.RESULT_FALLBACK -> showPluginEditor()
            }
            REQUEST_UNSAVED_CHANGES -> when (resultCode) {
                DialogInterface.BUTTON_POSITIVE -> saveAndExit()
                DialogInterface.BUTTON_NEGATIVE -> requireActivity().finish()
            }
            else -> super.onActivityResult(requestCode, resultCode, data)
        }
    }

    override fun onOptionsItemSelected(item: MenuItem) = when (item.itemId) {
        R.id.action_delete -> {
            DeleteConfirmationDialogFragment().withArg(ProfileIdArg(profileId)).show(this)
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
