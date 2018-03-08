package com.github.shadowsocks

import java.util.Locale

import android.content.SharedPreferences.OnSharedPreferenceChangeListener
import android.content.{Intent, SharedPreferences}
import android.net.Uri
import android.os.{Build, Bundle}
import java.io.{IOException, File}
import java.net.URL
import android.preference.{Preference, PreferenceFragment, SwitchPreference}
import android.support.design.widget.Snackbar
import android.support.v7.app.AlertDialog
import android.app.ProgressDialog
import android.content._
import android.view.View
import android.webkit.{WebView, WebViewClient}
import android.widget._
import android.os.Looper
import com.github.shadowsocks.ShadowsocksApplication.app
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.preferences._
import com.github.shadowsocks.utils.{Key, TcpFastOpen, Utils}
import com.github.shadowsocks.utils.CloseUtils._
import com.github.shadowsocks.utils.IOUtils
import android.content.Context
import com.github.shadowsocks.utils._

import java.io.InputStreamReader
import java.io.BufferedReader
import android.util.Log

object ShadowsocksSettings {
  // Constants
  private final val TAG = "ShadowsocksSettings"
  private val PROXY_PREFS = Array(Key.group_name, Key.name, Key.host, Key.remotePort, Key.localPort, Key.password, Key.method,
    Key.protocol, Key.obfs, Key.obfs_param, Key.dns, Key.china_dns, Key.protocol_param)
  private val FEATURE_PREFS = Array(Key.route, Key.proxyApps, Key.udpdns, Key.ipv6, Key.tfo)

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
      case Key.group_name => updateSummaryEditTextPreference(pref, profile.url_group)
      case Key.name => updateSummaryEditTextPreference(pref, profile.name)
      case Key.remotePort => updateNumberPickerPreference(pref, profile.remotePort)
      case Key.localPort => updateNumberPickerPreference(pref, profile.localPort)
      case Key.password => updatePasswordEditTextPreference(pref, profile.password)
      case Key.method => updateDropDownPreference(pref, profile.method)
      case Key.protocol => updateDropDownPreference(pref, profile.protocol)
      case Key.protocol_param => updateSummaryEditTextPreference(pref, profile.protocol_param)
      case Key.obfs => updateDropDownPreference(pref, profile.obfs)
      case Key.obfs_param => updateSummaryEditTextPreference(pref, profile.obfs_param)
      case Key.route => updateDropDownPreference(pref, profile.route)
      case Key.proxyApps => updateSwitchPreference(pref, profile.proxyApps)
      case Key.udpdns => updateSwitchPreference(pref, profile.udpdns)
      case Key.dns => updateSummaryEditTextPreference(pref, profile.dns)
      case Key.china_dns => updateSummaryEditTextPreference(pref, profile.china_dns)
      case Key.ipv6 => updateSwitchPreference(pref, profile.ipv6)
      case _ => {}
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

    findPreference(Key.group_name).setOnPreferenceChangeListener((_, value) => {
      profile.url_group = value.asInstanceOf[String]
      app.profileManager.updateProfile(profile)
    })
    findPreference(Key.name).setOnPreferenceChangeListener((_, value) => {
      profile.name = value.asInstanceOf[String]
      app.profileManager.updateProfile(profile)
    })
    findPreference(Key.host).setOnPreferenceClickListener((preference: Preference) => {
      val HostEditText = new EditText(activity);
      HostEditText.setText(profile.host);
      new AlertDialog.Builder(activity)
        .setTitle(getString(R.string.proxy))
        .setPositiveButton(android.R.string.ok, ((_, _) => {
          profile.host = HostEditText.getText().toString()
          app.profileManager.updateProfile(profile)
        }): DialogInterface.OnClickListener)
        .setNegativeButton(android.R.string.no,  ((_, _) => {
          setProfile(profile)
        }): DialogInterface.OnClickListener)
        .setView(HostEditText)
        .create()
        .show()
      true
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
    findPreference(Key.protocol).setOnPreferenceChangeListener((_, value) => {
      profile.protocol = value.asInstanceOf[String]
      app.profileManager.updateProfile(profile)
    })
    findPreference(Key.protocol_param).setOnPreferenceChangeListener((_, value) => {
      profile.protocol_param = value.asInstanceOf[String]
      app.profileManager.updateProfile(profile)
    })
    findPreference(Key.obfs).setOnPreferenceChangeListener((_, value) => {
      profile.obfs = value.asInstanceOf[String]
      app.profileManager.updateProfile(profile)
    })
    findPreference(Key.obfs_param).setOnPreferenceChangeListener((_, value) => {
      profile.obfs_param = value.asInstanceOf[String]
      app.profileManager.updateProfile(profile)
    })
    findPreference(Key.route).setOnPreferenceChangeListener((_, value) => {
      if(value == "self") {
        val AclUrlEditText = new EditText(activity);
        AclUrlEditText.setText(getPreferenceManager.getSharedPreferences.getString(Key.aclurl, ""));
        new AlertDialog.Builder(activity)
          .setTitle(getString(R.string.acl_file))
          .setPositiveButton(android.R.string.ok, ((_, _) => {
            if(AclUrlEditText.getText().toString() == "")
            {
              setProfile(profile)
            }
            else
            {
              getPreferenceManager.getSharedPreferences.edit.putString(Key.aclurl, AclUrlEditText.getText().toString()).commit()
              downloadAcl(AclUrlEditText.getText().toString())
              app.profileManager.updateAllProfile_String(Key.route, value.asInstanceOf[String])
            }
          }): DialogInterface.OnClickListener)
          .setNegativeButton(android.R.string.no,  ((_, _) => {
            setProfile(profile)
          }): DialogInterface.OnClickListener)
          .setView(AclUrlEditText)
          .create()
          .show()
      }
      else {
        app.profileManager.updateAllProfile_String(Key.route, value.asInstanceOf[String])
      }

      true
    })

    isProxyApps = findPreference(Key.proxyApps).asInstanceOf[SwitchPreference]
    isProxyApps.setOnPreferenceClickListener(_ => {
      startActivity(new Intent(activity, classOf[AppManager]))
      isProxyApps.setChecked(true)
      false
    })
    isProxyApps.setOnPreferenceChangeListener((_, value) => {
      app.profileManager.updateAllProfile_Boolean("proxyApps", value.asInstanceOf[Boolean])
    })

    findPreference(Key.udpdns).setOnPreferenceChangeListener((_, value) => {
      app.profileManager.updateAllProfile_Boolean("udpdns", value.asInstanceOf[Boolean])
    })
    findPreference(Key.dns).setOnPreferenceChangeListener((_, value) => {
      app.profileManager.updateAllProfile_String(Key.dns, value.asInstanceOf[String])
    })
    findPreference(Key.china_dns).setOnPreferenceChangeListener((_, value) => {
      app.profileManager.updateAllProfile_String(Key.china_dns, value.asInstanceOf[String])
    })
    findPreference(Key.ipv6).setOnPreferenceChangeListener((_, value) => {
      app.profileManager.updateAllProfile_Boolean("ipv6", value.asInstanceOf[Boolean])
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
    tfo.setOnPreferenceChangeListener((_, v) => {
      new Thread {
        override def run() {
          val value = v.asInstanceOf[Boolean]
          val result = TcpFastOpen.enabled(value)
          if (result != null && result != "Success.")
            activity.handler.post(() => {
              Snackbar.make(activity.findViewById(android.R.id.content), result, Snackbar.LENGTH_LONG).show()
            })
        }
      }.start
      true
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

    findPreference("ignore_battery_optimization").setOnPreferenceClickListener((preference: Preference) => {
      app.track(TAG, "ignore_battery_optimization")
      activity.ignoreBatteryOptimization()
      true
    })

    findPreference("aclupdate").setOnPreferenceClickListener((preference: Preference) => {
      app.track(TAG, "aclupdate")
      val url = getPreferenceManager.getSharedPreferences.getString(Key.aclurl, "");
      if(url == "")
      {
        new AlertDialog.Builder(activity)
          .setTitle(getString(R.string.aclupdate).formatLocal(Locale.ENGLISH, BuildConfig.VERSION_NAME))
          .setNegativeButton(getString(android.R.string.ok), null)
          .setMessage(R.string.aclupdate_url_notset)
          .create()
          .show()
      }
      else
      {
        downloadAcl(url)
      }
      true
    })

    if(new File(app.getApplicationInfo.dataDir + '/' + "self.acl").exists == false && getPreferenceManager.getSharedPreferences.getString(Key.aclurl, "") != "")
    {
      downloadAcl(getPreferenceManager.getSharedPreferences.getString(Key.aclurl, ""))
    }

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

    findPreference("logcat").setOnPreferenceClickListener((preference: Preference) => {
      app.track(TAG, "logcat")

      val et_logcat = new EditText(activity)

      try {
        val logcat = Runtime.getRuntime().exec("logcat -d")
        val br = new BufferedReader(new InputStreamReader(logcat.getInputStream()))
        var line = ""
        line = br.readLine()
        while (line != null) {
            et_logcat.append(line)
            et_logcat.append("\n")
            line = br.readLine()
        }
        br.close()
      } catch {
        case e: Exception =>  // unknown failures, probably shouldn't retry
          e.printStackTrace()
      }

      new AlertDialog.Builder(activity)
        .setTitle("Logcat")
        .setNegativeButton(getString(android.R.string.ok), null)
        .setView(et_logcat)
        .create()
        .show()
      true
    })

    findPreference(Key.frontproxy).setOnPreferenceClickListener((preference: Preference) => {
      val prefs = getPreferenceManager.getSharedPreferences()

      val view = View.inflate(activity, R.layout.layout_front_proxy, null);
      val sw_frontproxy_enable = view.findViewById(R.id.sw_frontproxy_enable).asInstanceOf[Switch]
      val sp_frontproxy_type = view.findViewById(R.id.sp_frontproxy_type).asInstanceOf[Spinner]
      val et_frontproxy_addr = view.findViewById(R.id.et_frontproxy_addr).asInstanceOf[EditText]
      val et_frontproxy_port = view.findViewById(R.id.et_frontproxy_port).asInstanceOf[EditText]
      val et_frontproxy_username = view.findViewById(R.id.et_frontproxy_username).asInstanceOf[EditText]
      val et_frontproxy_password = view.findViewById(R.id.et_frontproxy_password).asInstanceOf[EditText]

      sp_frontproxy_type.setSelection(getResources().getStringArray(R.array.frontproxy_type_entry).indexOf(prefs.getString("frontproxy_type", "socks5")))

      if (prefs.getInt("frontproxy_enable", 0) == 1) {
        sw_frontproxy_enable.setChecked(true)
      }

      et_frontproxy_addr.setText(prefs.getString("frontproxy_addr", ""))
      et_frontproxy_port.setText(prefs.getString("frontproxy_port", ""))
      et_frontproxy_username.setText(prefs.getString("frontproxy_username", ""))
      et_frontproxy_password.setText(prefs.getString("frontproxy_password", ""))

      sw_frontproxy_enable.setOnCheckedChangeListener(((_, isChecked: Boolean) => {
        val prefs_edit = prefs.edit()
        if (isChecked) {
          prefs_edit.putInt("frontproxy_enable", 1)
          if (!new File(app.getApplicationInfo.dataDir + "/proxychains.conf").exists) {
            val proxychains_conf = ConfigUtils
              .PROXYCHAINS.formatLocal(Locale.ENGLISH, prefs.getString("frontproxy_type", "socks5")
                                                    , prefs.getString("frontproxy_addr", "")
                                                    , prefs.getString("frontproxy_port", "")
                                                    , prefs.getString("frontproxy_username", "")
                                                    , prefs.getString("frontproxy_password", ""))
            Utils.printToFile(new File(app.getApplicationInfo.dataDir + "/proxychains.conf"))(p => {
              p.println(proxychains_conf)
            })
          }
        } else {
          prefs_edit.putInt("frontproxy_enable", 0)
          if (new File(app.getApplicationInfo.dataDir + "/proxychains.conf").exists) {
            new File(app.getApplicationInfo.dataDir + "/proxychains.conf").delete
          }
        }
        prefs_edit.apply()
      }): CompoundButton.OnCheckedChangeListener)

      new AlertDialog.Builder(activity)
        .setTitle(getString(R.string.frontproxy_set))
        .setPositiveButton(android.R.string.ok, ((_, _) => {
          val prefs_edit = prefs.edit()
          prefs_edit.putString("frontproxy_type", sp_frontproxy_type.getSelectedItem().toString())

          prefs_edit.putString("frontproxy_addr", et_frontproxy_addr.getText().toString())
          prefs_edit.putString("frontproxy_port", et_frontproxy_port.getText().toString())
          prefs_edit.putString("frontproxy_username", et_frontproxy_username.getText().toString())
          prefs_edit.putString("frontproxy_password", et_frontproxy_password.getText().toString())

          prefs_edit.apply()

          if (new File(app.getApplicationInfo.dataDir + "/proxychains.conf").exists) {
            val proxychains_conf = ConfigUtils
              .PROXYCHAINS.formatLocal(Locale.ENGLISH, prefs.getString("frontproxy_type", "socks5")
                                                    , prefs.getString("frontproxy_addr", "")
                                                    , prefs.getString("frontproxy_port", "")
                                                    , prefs.getString("frontproxy_username", "")
                                                    , prefs.getString("frontproxy_password", ""))
            Utils.printToFile(new File(app.getApplicationInfo.dataDir + "/proxychains.conf"))(p => {
              p.println(proxychains_conf)
            })
          }
        }): DialogInterface.OnClickListener)
        .setNegativeButton(android.R.string.no, null)
        .setView(view)
        .create()
        .show()
      true
    })
  }

  def downloadAcl(url: String) {
    val progressDialog = ProgressDialog.show(activity, getString(R.string.aclupdate), getString(R.string.aclupdate_downloading), false, false)
    new Thread {
      override def run() {
        Looper.prepare();
        try {
          IOUtils.writeString(app.getApplicationInfo.dataDir + '/' + "self.acl", autoClose(
            new URL(url).openConnection().getInputStream())(IOUtils.readString))
          progressDialog.dismiss()
          new AlertDialog.Builder(activity, R.style.Theme_Material_Dialog_Alert)
            .setTitle(getString(R.string.aclupdate))
            .setNegativeButton(android.R.string.yes, null)
            .setMessage(getString(R.string.aclupdate_successfully))
            .create()
            .show()
        } catch {
          case e: IOException =>
            e.printStackTrace()
            progressDialog.dismiss()
            new AlertDialog.Builder(activity, R.style.Theme_Material_Dialog_Alert)
              .setTitle(getString(R.string.aclupdate))
              .setNegativeButton(android.R.string.yes, null)
              .setMessage(getString(R.string.aclupdate_failed))
              .create()
              .show()
          case e: Exception =>  // unknown failures, probably shouldn't retry
            e.printStackTrace()
            progressDialog.dismiss()
            new AlertDialog.Builder(activity, R.style.Theme_Material_Dialog_Alert)
              .setTitle(getString(R.string.aclupdate))
              .setNegativeButton(android.R.string.yes, null)
              .setMessage(getString(R.string.aclupdate_failed))
              .create()
              .show()
        }
        Looper.loop();
      }
    }.start()
  }

  def refreshProfile() {
    profile = app.currentProfile match {
      case Some(p) => p
      case None =>
        app.profileManager.getFirstProfile match {
          case Some(p) =>
            app.profileId(p.id)
            p
          case None =>
            val default = app.profileManager.createDefault()
            app.profileId(default.id)
            default
        }
    }

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
