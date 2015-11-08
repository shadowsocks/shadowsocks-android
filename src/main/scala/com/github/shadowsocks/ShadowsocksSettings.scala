package com.github.shadowsocks

import android.os.Bundle
import android.preference.{Preference, PreferenceFragment}

// TODO: Move related logic here
class ShadowsocksSettings extends PreferenceFragment {
  private lazy val activity = getActivity.asInstanceOf[Shadowsocks]

  override def onCreate(savedInstanceState: Bundle) {
    super.onCreate(savedInstanceState)
    addPreferencesFromResource(R.xml.pref_all)
    findPreference("recovery").setOnPreferenceClickListener((preference: Preference) => {
      ShadowsocksApplication.track(Shadowsocks.TAG, "reset")
      activity.recovery()
      true
    })
    findPreference("flush_dnscache").setOnPreferenceClickListener((preference: Preference) => {
      ShadowsocksApplication.track(Shadowsocks.TAG, "flush_dnscache")
      activity.flushDnsCache()
      true
    })
  }
}
