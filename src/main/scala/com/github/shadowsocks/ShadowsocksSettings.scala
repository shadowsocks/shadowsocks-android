package com.github.shadowsocks

import android.os.Bundle
import android.preference.PreferenceFragment

// TODO: Move related logic here
class ShadowsocksSettings extends PreferenceFragment {
  override def onCreate(savedInstanceState: Bundle) {
    super.onCreate(savedInstanceState)
    addPreferencesFromResource(R.xml.pref_all)
  }
}
