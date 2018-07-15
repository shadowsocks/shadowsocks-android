package com.github.shadowsocks.controllers

import android.annotation.SuppressLint
import android.app.Activity
import android.content.BroadcastReceiver
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.os.UserManager
import android.support.design.widget.Snackbar
import android.support.v7.app.AlertDialog
import android.support.v7.preference.Preference
import android.support.v7.preference.PreferenceDataStore
import android.support.v7.preference.SwitchPreference
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import com.bluelinelabs.conductor.ControllerChangeHandler
import com.bluelinelabs.conductor.ControllerChangeType
import com.github.shadowsocks.App.Companion.app
import com.github.shadowsocks.AppManager
import com.github.shadowsocks.ProfileConfigActivity
import com.github.shadowsocks.R
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.database.ProfileManager
import com.github.shadowsocks.plugin.PluginConfiguration
import com.github.shadowsocks.plugin.PluginContract
import com.github.shadowsocks.plugin.PluginManager
import com.github.shadowsocks.plugin.PluginOptions
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.preference.IconListPreference
import com.github.shadowsocks.preference.OnPreferenceDataStoreChangeListener
import com.github.shadowsocks.utils.Action
import com.github.shadowsocks.utils.DirectBoot
import com.github.shadowsocks.utils.Key
import im.mash.preference.EditTextPreference
import im.mash.preference.PreferenceController

class ProfileConfigController : PreferenceController(), Preference.OnPreferenceChangeListener, OnPreferenceDataStoreChangeListener {

    companion object {
        const val TAG = "ProfileConfigController"
        @SuppressLint("StaticFieldLeak")
        var instance: ProfileConfigController? = null
        private const val REQUEST_CODE_PLUGIN_CONFIGURE = 1
    }

    private var profileId = -1L
    private lateinit var isProxyApps: SwitchPreference
    private lateinit var plugin: IconListPreference
    private lateinit var pluginConfigure: EditTextPreference
    private lateinit var pluginConfiguration: PluginConfiguration
    private lateinit var receiver: BroadcastReceiver

    init {
        setHasOptionsMenu(true)
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        preferenceManager!!.preferenceDataStore = DataStore.privateStore
        val activity = activity as ProfileConfigActivity
        profileId = activity.intent.getLongExtra(Action.EXTRA_PROFILE_ID, -1L)
        addPreferencesFromResource(R.xml.pref_profile)
        instance = this
        if (Build.VERSION.SDK_INT >= 25 && activity.getSystemService(UserManager::class.java).isDemoUser) {
            findPreference(Key.host)?.summary = "shadowsocks.example.org"
            findPreference(Key.remotePort)?.summary = "1337"
            findPreference(Key.password)?.summary = "\u2022".repeat(32)
        }
        val serviceMode = DataStore.serviceMode
        findPreference(Key.remoteDns)?.isEnabled = serviceMode != Key.modeProxy
        isProxyApps = findPreference(Key.proxyApps) as SwitchPreference
        isProxyApps.isEnabled = serviceMode == Key.modeVpn
        isProxyApps.setOnPreferenceClickListener {
            startActivity(Intent(activity, AppManager::class.java))
            isProxyApps.isChecked = true
            false
        }
        findPreference(Key.udpdns)?.isEnabled = serviceMode != Key.modeProxy
        plugin = findPreference(Key.plugin) as IconListPreference
        pluginConfigure = findPreference(Key.pluginConfigure) as EditTextPreference
        plugin.unknownValueSummary = resources!!.getString(R.string.plugin_unknown)
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
        receiver = app.listenForPackageChanges(false) { initPlugins() }
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
        PluginConfigurationDialogController.newInstance(Key.pluginConfigure, pluginConfiguration.selected)
    }

    override fun onDisplayPreferenceDialog(preference: Preference) {
        if (preference.key == Key.pluginConfigure) {
            val intent = PluginManager.buildIntent(pluginConfiguration.selected, PluginContract.ACTION_CONFIGURE)
            if (intent.resolveActivity(activity!!.packageManager) == null) showPluginEditor() else
                startActivityForResult(intent
                        .putExtra(PluginContract.EXTRA_OPTIONS, pluginConfiguration.selectedOptions.toString())
                        .putExtra(PluginContract.EXTRA_NIGHT_MODE, DataStore.nightMode),
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

    fun saveAndExit() {
        val profile = ProfileManager.getProfile(profileId) ?: Profile()
        profile.id = profileId
        profile.deserialize()
        ProfileManager.updateProfile(profile)
        ProfilesController.instance?.profilesAdapter?.deepRefreshId(profileId)
        if (DataStore.profileId == profileId && DataStore.directBootAware) DirectBoot.update()
        activity!!.finish()
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
        if (key != null && key != Key.proxyApps && findPreference(key) != null) DataStore.dirty = true
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        when (item.itemId) {
            R.id.action_delete -> {
                val activity = activity as ProfileConfigActivity
                AlertDialog.Builder(activity)
                        .setTitle(R.string.delete_confirm_prompt)
                        .setPositiveButton(R.string.yes) { _, _ ->
                            ProfileManager.delProfile(profileId)
                            activity.finish()
                        }
                        .setNegativeButton(R.string.no, null)
                        .create()
                        .show()
            }
            R.id.action_apply -> {
                saveAndExit()
            }
        }
        return super.onOptionsItemSelected(item)
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        super.onCreateOptionsMenu(menu, inflater)
        menu.clear()
        inflater.inflate(R.menu.profile_config_menu, menu)
    }

    override fun onChangeStarted(changeHandler: ControllerChangeHandler, changeType: ControllerChangeType) {
        setOptionsMenuHidden(!changeType.isEnter)
        if (changeType.isEnter) {
            isProxyApps.isChecked = DataStore.proxyApps // fetch proxyApps updated by AppManager
        }
    }

    override fun onDestroy() {
        DataStore.privateStore.unregisterChangeListener(this)
        app.unregisterReceiver(receiver)
        instance = null
        super.onDestroy()
    }
    override fun onCreateItemDecoration(): DividerDecoration {
        return CategoryDivideDividerDecoration()
    }
}