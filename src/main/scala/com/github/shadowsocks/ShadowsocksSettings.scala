package com.github.shadowsocks

import java.util.Locale

import android.content.SharedPreferences.OnSharedPreferenceChangeListener
import android.content.{Intent, SharedPreferences}
import android.net.Uri
import android.os.{Build, Bundle}
import android.preference.{Preference, PreferenceFragment, SwitchPreference}
import android.support.v7.app.AlertDialog
import android.webkit.{WebView, WebViewClient}
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.preferences._
import com.github.shadowsocks.utils.{Key, Utils}

object ShadowsocksSettings {
  // Constants
  val PREFS_NAME = "Shadowsocks"
  val PROXY_PREFS = Array(Key.profileName, Key.proxy, Key.remotePort, Key.localPort, Key.sitekey, Key.encMethod,
    Key.isAuth)
  val FEATURE_PREFS = Array(Key.route, Key.isProxyApps, Key.isUdpDns, Key.isIpv6)

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
      case Key.profileName => updateSummaryEditTextPreference(pref, profile.name)
      case Key.proxy => updateSummaryEditTextPreference(pref, profile.host)
      case Key.remotePort => updateNumberPickerPreference(pref, profile.remotePort)
      case Key.localPort => updateNumberPickerPreference(pref, profile.localPort)
      case Key.sitekey => updatePasswordEditTextPreference(pref, profile.password)
      case Key.encMethod => updateDropDownPreference(pref, profile.method)
      case Key.route => updateDropDownPreference(pref, profile.route)
      case Key.isProxyApps => updateSwitchPreference(pref, profile.proxyApps)
      case Key.isUdpDns => updateSwitchPreference(pref, profile.udpdns)
      case Key.isAuth => updateSwitchPreference(pref, profile.auth)
      case Key.isIpv6 => updateSwitchPreference(pref, profile.ipv6)
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

    isProxyApps = findPreference(Key.isProxyApps).asInstanceOf[SwitchPreference]
    isProxyApps.setOnPreferenceClickListener((preference: Preference) => {
      startActivity(new Intent(activity, classOf[AppManager]))
      isProxyApps.setChecked(true)
      false
    })

    findPreference("recovery").setOnPreferenceClickListener((preference: Preference) => {
      ShadowsocksApplication.track(Shadowsocks.TAG, "reset")
      activity.recovery()
      true
    })

    val flush = findPreference("flush_dnscache")
    if (Build.VERSION.SDK_INT < 17) flush.setSummary(R.string.flush_dnscache_summary)
    flush.setOnPreferenceClickListener(_ => {
      ShadowsocksApplication.track(Shadowsocks.TAG, "flush_dnscache")
      activity.flushDnsCache()
      true
    })

    findPreference("about").setOnPreferenceClickListener((preference: Preference) => {
      ShadowsocksApplication.track(Shadowsocks.TAG, "about")
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
        .setTitle(getString(R.string.about_title).formatLocal(Locale.ENGLISH, ShadowsocksApplication.getVersionName))
        .setNegativeButton(getString(android.R.string.ok), null)
        .setView(web)
        .create()
        .show()
      true
    })
  }

  override def onResume {
    super.onResume()
    isProxyApps.setChecked(ShadowsocksApplication.settings.getBoolean(Key.isProxyApps, false))  // update
  }

  override def onDestroy {
    super.onDestroy()
    ShadowsocksApplication.settings.unregisterOnSharedPreferenceChangeListener(this)
  }

  def onSharedPreferenceChanged(sharedPreferences: SharedPreferences, key: String) = key match {
    case Key.isNAT =>
      activity.handler.post(() => {
        activity.deattachService
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
        (name != Key.isProxyApps || Utils.isLollipopOrAbove || !ShadowsocksApplication.isVpnEnabled))
    }
  }

  def update(profile: Profile) {
    for (name <- PROXY_PREFS) {
      val pref = findPreference(name)
      updatePreference(pref, name, profile)
    }
    for (name <- FEATURE_PREFS) {
      val pref = findPreference(name)
      updatePreference(pref, name, profile)
    }
  }
}
