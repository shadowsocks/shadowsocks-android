package com.github.shadowsocks

import java.util.Locale

import android.content.SharedPreferences.OnSharedPreferenceChangeListener
import android.content.{Intent, SharedPreferences}
import android.net.Uri
import android.os.Bundle
import android.preference.{Preference, PreferenceFragment, SwitchPreference}
import android.support.design.widget.Snackbar
import android.support.v7.app.AlertDialog
import android.webkit.{WebView, WebViewClient}
import com.github.shadowsocks.ShadowsocksApplication.app
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.preferences._
import com.github.shadowsocks.utils.{Key, TcpFastOpen, Utils}

object ShadowsocksSettings {
  // Constants
  private final val TAG = "ShadowsocksSettings"
  private val PROXY_PREFS = Array(Key.name, Key.host, Key.remotePort, Key.localPort, Key.password, Key.method,
    Key.auth, Key.kcp, Key.kcpPort, Key.kcpcli)
  private val FEATURE_PREFS = Array(Key.route, Key.proxyApps, Key.udpdns, Key.ipv6)

  // Helper functions
  def updateDropDownPreference(pref: Preference, value: String) {
    pref.asInstanceOf[DropDownPreference].setValue(value)
  }

  def updatePasswordEditTextPreference(pref: Preference, value: String) {
    pref.setSummary(value)
    pref.asInstanceOf[PasswordEditTextPreference].setText(value)
  }

  def updateNumberPickerPreference(pref: Preference, value: Int) {
    pref.asInstanceOf[NumberPickerPreference].setValue(value)
  }

  def updateSummaryEditTextPreference(pref: Preference, value: String) {
    pref.setSummary(value)
    pref.asInstanceOf[SummaryEditTextPreference].setText(value)
  }

  def updateSwitchPreference(pref: Preference, value: Boolean) {
    pref.asInstanceOf[SwitchPreference].setChecked(value)
  }

  def updatePreference(pref: Preference, name: String, profile: Profile) {
    name match {
      case Key.name => updateSummaryEditTextPreference(pref, profile.name)
      case Key.host => updateSummaryEditTextPreference(pref, profile.host)
      case Key.remotePort => updateNumberPickerPreference(pref, profile.remotePort)
      case Key.localPort => updateNumberPickerPreference(pref, profile.localPort)
      case Key.password => updatePasswordEditTextPreference(pref, profile.password)
      case Key.method => updateDropDownPreference(pref, profile.method)
      case Key.route => updateDropDownPreference(pref, profile.route)
      case Key.proxyApps => updateSwitchPreference(pref, profile.proxyApps)
      case Key.udpdns => updateSwitchPreference(pref, profile.udpdns)
      case Key.auth => updateSwitchPreference(pref, profile.auth)
      case Key.ipv6 => updateSwitchPreference(pref, profile.ipv6)
      case Key.kcp => updateSwitchPreference(pref, profile.kcp)
      case Key.kcpPort => updateNumberPickerPreference(pref, profile.kcpPort)
      case Key.kcpcli => updateSummaryEditTextPreference(pref, profile.kcpcli)
    }
  }
}

class ShadowsocksSettings extends PreferenceFragment with OnSharedPreferenceChangeListener {
  import ShadowsocksSettings._

  private def activity = getActivity.asInstanceOf[Shadowsocks]
  lazy val natSwitch = findPreference(Key.isNAT).asInstanceOf[SwitchPreference]

  private var isProxyApps: SwitchPreference = _

  override def onCreate(savedInstanceState: Bundle) {
    super.onCreate(savedInstanceState)
    addPreferencesFromResource(R.xml.pref_all)
    getPreferenceManager.getSharedPreferences.registerOnSharedPreferenceChangeListener(this)

    findPreference(Key.name).setOnPreferenceChangeListener((_, value) => {
      profile.name = value.asInstanceOf[String]
      app.profileManager.updateProfile(profile)
    })
    findPreference(Key.host).setOnPreferenceChangeListener((_, value) => {
      profile.host = value.asInstanceOf[String]
      app.profileManager.updateProfile(profile)
    })
    findPreference(Key.remotePort).setOnPreferenceChangeListener((_, value) => {
      profile.remotePort = value.asInstanceOf[Int]
      app.profileManager.updateProfile(profile)
    })
    findPreference(Key.localPort).setOnPreferenceChangeListener((_, value) => {
      profile.localPort = value.asInstanceOf[Int]
      app.profileManager.updateProfile(profile)
    })
    findPreference(Key.password).setOnPreferenceChangeListener((_, value) => {
      profile.password = value.asInstanceOf[String]
      app.profileManager.updateProfile(profile)
    })
    findPreference(Key.method).setOnPreferenceChangeListener((_, value) => {
      profile.method = value.asInstanceOf[String]
      app.profileManager.updateProfile(profile)
    })
    findPreference(Key.route).setOnPreferenceChangeListener((_, value) => {
      profile.route = value.asInstanceOf[String]
      app.profileManager.updateProfile(profile)
    })

    findPreference(Key.kcp).setOnPreferenceChangeListener((_, value) => {
      profile.kcp = value.asInstanceOf[Boolean]
      app.profileManager.updateProfile(profile)
    })
    findPreference(Key.kcpPort).setOnPreferenceChangeListener((_, value) => {
      profile.kcpPort = value.asInstanceOf[Int]
      app.profileManager.updateProfile(profile)
    })
    findPreference(Key.kcpcli).setOnPreferenceChangeListener((_, value) => {
      profile.kcpcli = value.asInstanceOf[String]
      app.profileManager.updateProfile(profile)
    })

    isProxyApps = findPreference(Key.proxyApps).asInstanceOf[SwitchPreference]
    isProxyApps.setOnPreferenceClickListener(_ => {
      startActivity(new Intent(activity, classOf[AppManager]))
      isProxyApps.setChecked(true)
      false
    })
    isProxyApps.setOnPreferenceChangeListener((_, value) => {
      profile.proxyApps = value.asInstanceOf[Boolean]
      app.profileManager.updateProfile(profile)
    })

    findPreference(Key.udpdns).setOnPreferenceChangeListener((_, value) => {
      profile.udpdns = value.asInstanceOf[Boolean]
      app.profileManager.updateProfile(profile)
    })
    findPreference(Key.auth).setOnPreferenceChangeListener((_, value) => {
      profile.auth = value.asInstanceOf[Boolean]
      app.profileManager.updateProfile(profile)
    })
    findPreference(Key.ipv6).setOnPreferenceChangeListener((_, value) => {
      profile.ipv6 = value.asInstanceOf[Boolean]
      app.profileManager.updateProfile(profile)
    })

    val switch = findPreference(Key.isAutoConnect).asInstanceOf[SwitchPreference]
    switch.setOnPreferenceChangeListener((_, value) => {
      BootReceiver.setEnabled(activity, value.asInstanceOf[Boolean])
      true
    })
    if (getPreferenceManager.getSharedPreferences.getBoolean(Key.isAutoConnect, false)) {
      BootReceiver.setEnabled(activity, true)
      getPreferenceManager.getSharedPreferences.edit.remove(Key.isAutoConnect).apply
    }
    switch.setChecked(BootReceiver.getEnabled(activity))

    val tfo = findPreference(Key.tfo).asInstanceOf[SwitchPreference]
    tfo.setChecked(TcpFastOpen.sendEnabled)
    tfo.setOnPreferenceChangeListener((_, v) => {
      val value = v.asInstanceOf[Boolean]
      val result = TcpFastOpen.enabled(value)
      if (result != null && result != "Success.")
        Snackbar.make(activity.findViewById(android.R.id.content), result, Snackbar.LENGTH_LONG).show()
      value == TcpFastOpen.sendEnabled
    })
    if (!TcpFastOpen.supported) {
      tfo.setEnabled(false)
      tfo.setSummary(getString(R.string.tcp_fastopen_summary_unsupported, java.lang.System.getProperty("os.version")))
    }

    findPreference("recovery").setOnPreferenceClickListener((preference: Preference) => {
      app.track(TAG, "reset")
      activity.recovery()
      true
    })

    findPreference("flush_dnscache").setOnPreferenceClickListener(_ => {
      app.track(TAG, "flush_dnscache")
      activity.flushDnsCache()
      true
    })

    findPreference("about").setOnPreferenceClickListener((preference: Preference) => {
      app.track(TAG, "about")
      val web = new WebView(activity)
      web.loadUrl("file:///android_asset/pages/about.html")
      web.setWebViewClient(new WebViewClient() {
        override def shouldOverrideUrlLoading(view: WebView, url: String): Boolean = {
          try {
            startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(url)))
          } catch {
            case _: android.content.ActivityNotFoundException => // Ignore
          }
          true
        }
      })

      new AlertDialog.Builder(activity)
        .setTitle(getString(R.string.about_title).formatLocal(Locale.ENGLISH, BuildConfig.VERSION_NAME))
        .setNegativeButton(getString(android.R.string.ok), null)
        .setView(web)
        .create()
        .show()
      true
    })
  }

  def refreshProfile() {
    profile = app.currentProfile.get
    isProxyApps.setChecked(profile.proxyApps)
  }

  override def onDestroy {
    super.onDestroy()
    app.settings.unregisterOnSharedPreferenceChangeListener(this)
  }

  def onSharedPreferenceChanged(pref: SharedPreferences, key: String) = key match {
    case Key.isNAT =>
      activity.handler.post(() => {
        activity.detachService
        activity.attachService
      })
    case _ =>
  }

  private var enabled = true
  def setEnabled(enabled: Boolean) {
    this.enabled = enabled
    for (name <- Key.isNAT #:: PROXY_PREFS.toStream #::: FEATURE_PREFS.toStream) {
      val pref = findPreference(name)
      if (pref != null) pref.setEnabled(enabled &&
        (name != Key.proxyApps || Utils.isLollipopOrAbove || app.isNatEnabled))
    }
  }

  var profile: Profile = _
  def setProfile(profile: Profile) {
    this.profile = profile
    for (name <- Array(PROXY_PREFS, FEATURE_PREFS).flatten) updatePreference(findPreference(name), name, profile)
  }
}
