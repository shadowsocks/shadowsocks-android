/*******************************************************************************/
/*                                                                             */
/*  Copyright (C) 2017 by Max Lv <max.c.lv@gmail.com>                          */
/*  Copyright (C) 2017 by Mygod Studio <contact-shadowsocks-android@mygod.be>  */
/*                                                                             */
/*  This program is free software: you can redistribute it and/or modify       */
/*  it under the terms of the GNU General Public License as published by       */
/*  the Free Software Foundation, either version 3 of the License, or          */
/*  (at your option) any later version.                                        */
/*                                                                             */
/*  This program is distributed in the hope that it will be useful,            */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of             */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              */
/*  GNU General Public License for more details.                               */
/*                                                                             */
/*  You should have received a copy of the GNU General Public License          */
/*  along with this program. If not, see <http://www.gnu.org/licenses/>.       */
/*                                                                             */
/*******************************************************************************/

package com.github.shadowsocks

import android.app.Activity
import android.content._
import android.os.{Build, Bundle, UserManager}
import android.support.design.widget.Snackbar
import android.support.v14.preference.SwitchPreference
import android.support.v7.app.AlertDialog
import android.support.v7.preference.{Preference, PreferenceDataStore}
import android.support.v7.widget.Toolbar.OnMenuItemClickListener
import android.text.TextUtils
import android.view.MenuItem
import com.github.shadowsocks.ShadowsocksApplication.app
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.plugin._
import com.github.shadowsocks.preference.{IconListPreference, OnPreferenceDataStoreChangeListener, PluginConfigurationDialogFragment}
import com.github.shadowsocks.utils.{Action, Key}
import com.takisoft.fix.support.v7.preference.{EditTextPreference, PreferenceFragmentCompatDividers}

object ProfileConfigFragment {
  private final val REQUEST_CODE_PLUGIN_CONFIGURE = 1
  private final val FRAGMENT_DIALOG_TAG = "android.support.v7.preference.PreferenceFragment.DIALOG"
}

class ProfileConfigFragment extends PreferenceFragmentCompatDividers with OnMenuItemClickListener
  with OnPreferenceDataStoreChangeListener {
  import ProfileConfigFragment._

  private var profile: Profile = _
  private var isProxyApps: SwitchPreference = _
  private var plugin: IconListPreference = _
  private var pluginConfigure: EditTextPreference = _
  private var pluginConfiguration: PluginConfiguration = _

  override def onCreatePreferencesFix(bundle: Bundle, key: String) {
    getPreferenceManager.setPreferenceDataStore(app.dataStore)
    app.profileManager.getProfile(getActivity.getIntent.getIntExtra(Action.EXTRA_PROFILE_ID, -1)) match {
      case Some(p) =>
        profile = p
        profile.serialize(app.dataStore)
      case None => getActivity.finish()
    }
    addPreferencesFromResource(R.xml.pref_profile)
    if (Build.VERSION.SDK_INT >= 25 && getActivity.getSystemService(classOf[UserManager]).isDemoUser) {
      findPreference(Key.host).setSummary("shadowsocks.example.org")
      findPreference(Key.remotePort).setSummary("1337")
      findPreference(Key.password).setSummary("\u2022" * 32)
    }
    isProxyApps = findPreference(Key.proxyApps).asInstanceOf[SwitchPreference]
    if (Build.VERSION.SDK_INT < 21) isProxyApps.getParent.removePreference(isProxyApps) else {
      isProxyApps.setEnabled(app.usingVpnMode)
      isProxyApps.setOnPreferenceClickListener(_ => {
        startActivity(new Intent(getActivity, classOf[AppManager]))
        isProxyApps.setChecked(true)
        false
      })
    }
    plugin = findPreference(Key.plugin).asInstanceOf[IconListPreference]
    pluginConfigure = findPreference(Key.pluginConfigure).asInstanceOf[EditTextPreference]
    plugin.unknownValueSummary = getString(R.string.plugin_unknown)
    plugin.setOnPreferenceChangeListener((_, value) => {
      val selected = value.asInstanceOf[String]
      pluginConfiguration = new PluginConfiguration(pluginConfiguration.pluginsOptions, selected)
      app.dataStore.plugin = pluginConfiguration.toString
      app.dataStore.dirty = true
      pluginConfigure.setEnabled(!TextUtils.isEmpty(selected))
      pluginConfigure.setText(pluginConfiguration.selectedOptions.toString)
      if (!PluginManager.fetchPlugins()(selected).trusted)
        Snackbar.make(getView, R.string.plugin_untrusted, Snackbar.LENGTH_LONG).show()
      true
    })
    pluginConfigure.setOnPreferenceChangeListener(onPluginConfigureChanged)
    initPlugins()
    app.listenForPackageChanges(initPlugins())
    app.dataStore.registerChangeListener(this)
  }

  def initPlugins() {
    val plugins = PluginManager.fetchPlugins()
    plugin.setEntries(plugins.map(_._2.label).toArray)
    plugin.setEntryValues(plugins.map(_._2.id.asInstanceOf[CharSequence]).toArray)
    plugin.setEntryIcons(plugins.map(_._2.icon).toArray)
    plugin.entryPackageNames = plugins.map(_._2.packageName).toArray
    pluginConfiguration = new PluginConfiguration(app.dataStore.plugin)
    plugin.setValue(pluginConfiguration.selected)
    plugin.init()
    plugin.checkSummary()
    pluginConfigure.setEnabled(!TextUtils.isEmpty(pluginConfiguration.selected))
    pluginConfigure.setText(pluginConfiguration.selectedOptions.toString)
  }

  private def onPluginConfigureChanged(p: Preference, value: Any) = try {
    val selected = pluginConfiguration.selected
    pluginConfiguration = new PluginConfiguration(pluginConfiguration.pluginsOptions +
      (pluginConfiguration.selected -> new PluginOptions(selected, value.asInstanceOf[String])), selected)
    app.dataStore.plugin = pluginConfiguration.toString
    app.dataStore.dirty = true
    true
  } catch {
    case exc: IllegalArgumentException =>
      Snackbar.make(getView, exc.getLocalizedMessage, Snackbar.LENGTH_LONG).show()
      false
  }

  def onPreferenceDataStoreChanged(store: PreferenceDataStore, key: String): Unit =
    if (key != Key.proxyApps && findPreference(key) != null) app.dataStore.dirty = true

  override def onDestroy() {
    app.dataStore.unregisterChangeListener(this)
    super.onDestroy()
  }

  override def onResume() {
    super.onResume()
    isProxyApps.setChecked(app.dataStore.proxyApps) // fetch proxyApps updated by AppManager
  }

  private def showPluginEditor() {
    val bundle = new Bundle()
    bundle.putString("key", Key.pluginConfigure)
    bundle.putString(PluginConfigurationDialogFragment.PLUGIN_ID_FRAGMENT_TAG, pluginConfiguration.selected)
    val fragment = new PluginConfigurationDialogFragment
    fragment.setArguments(bundle)
    fragment.setTargetFragment(this, 0)
    fragment.show(getFragmentManager, FRAGMENT_DIALOG_TAG)
  }

  override def onDisplayPreferenceDialog(preference: Preference): Unit = if (preference.getKey == Key.pluginConfigure) {
    val intent = PluginManager.buildIntent(pluginConfiguration.selected, PluginContract.ACTION_CONFIGURE)
    if (intent.resolveActivity(getActivity.getPackageManager) != null)
      startActivityForResult(intent.putExtra(PluginContract.EXTRA_OPTIONS,
        pluginConfiguration.selectedOptions.toString), REQUEST_CODE_PLUGIN_CONFIGURE) else {
      showPluginEditor()
    }
  } else super.onDisplayPreferenceDialog(preference)

  override def onActivityResult(requestCode: Int, resultCode: Int, data: Intent): Unit = requestCode match {
    case REQUEST_CODE_PLUGIN_CONFIGURE => resultCode match {
      case Activity.RESULT_OK =>
        val options = data.getStringExtra(PluginContract.EXTRA_OPTIONS)
        pluginConfigure.setText(options)
        onPluginConfigureChanged(null, options)
      case PluginContract.RESULT_FALLBACK => showPluginEditor()
      case _ =>
    }
    case _ => super.onActivityResult(requestCode, resultCode, data)
  }

  override def onMenuItemClick(item: MenuItem): Boolean = item.getItemId match {
    case R.id.action_delete =>
      new AlertDialog.Builder(getActivity)
        .setTitle(R.string.delete_confirm_prompt)
        .setPositiveButton(R.string.yes, ((_, _) => {
          app.profileManager.delProfile(profile.id)
          getActivity.finish()
        }): DialogInterface.OnClickListener)
        .setNegativeButton(R.string.no, null)
        .create()
        .show()
      true
    case R.id.action_apply =>
      saveAndExit()
      true
    case _ => false
  }

  def saveAndExit() {
    profile.deserialize(app.dataStore)
    app.profileManager.updateProfile(profile)
    if (ProfilesFragment.instance != null) ProfilesFragment.instance.profilesAdapter.deepRefreshId(profile.id)
    getActivity.finish()
  }
}
