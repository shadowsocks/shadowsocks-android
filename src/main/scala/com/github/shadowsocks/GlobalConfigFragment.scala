package com.github.shadowsocks

import android.os.Bundle
import android.support.design.widget.Snackbar
import android.support.v14.preference.SwitchPreference
import be.mygod.preference.PreferenceFragment
import com.github.shadowsocks.utils.{Key, TcpFastOpen}

class GlobalConfigFragment extends PreferenceFragment {
  override def onCreatePreferences(bundle: Bundle, key: String) {
    addPreferencesFromResource(R.xml.pref_global)
    val switch = findPreference(Key.isAutoConnect).asInstanceOf[SwitchPreference]
    switch.setOnPreferenceChangeListener((_, value) => {
      BootReceiver.setEnabled(getActivity, value.asInstanceOf[Boolean])
      true
    })
    if (getPreferenceManager.getSharedPreferences.getBoolean(Key.isAutoConnect, false)) {
      BootReceiver.setEnabled(getActivity, true)
      getPreferenceManager.getSharedPreferences.edit.remove(Key.isAutoConnect).apply()
    }
    switch.setChecked(BootReceiver.getEnabled(getActivity))

    val tfo = findPreference(Key.tfo).asInstanceOf[SwitchPreference]
    tfo.setChecked(TcpFastOpen.sendEnabled)
    tfo.setOnPreferenceChangeListener((_, v) => {
      val value = v.asInstanceOf[Boolean]
      val result = TcpFastOpen.enabled(value)
      if (result != null && result != "Success.")
        Snackbar.make(getActivity.findViewById(R.id.content), result, Snackbar.LENGTH_LONG).show()
      value == TcpFastOpen.sendEnabled
    })
    if (!TcpFastOpen.supported) {
      tfo.setEnabled(false)
      tfo.setSummary(getString(R.string.tcp_fastopen_summary_unsupported, java.lang.System.getProperty("os.version")))
    }
  }
}
