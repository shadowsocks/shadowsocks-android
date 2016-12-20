package com.github.shadowsocks

import android.app.ProgressDialog
import android.os.{Bundle, Handler, Message}
import android.support.design.widget.Snackbar
import android.support.v14.preference.SwitchPreference
import be.mygod.preference.PreferenceFragment
import com.github.shadowsocks.ShadowsocksApplication.app
import com.github.shadowsocks.utils.{Key, TcpFastOpen, Utils}

class GlobalConfigFragment extends PreferenceFragment {
  private var progressDialog: ProgressDialog = _

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
        Snackbar.make(getActivity.findViewById(android.R.id.content), result, Snackbar.LENGTH_LONG).show()
      value == TcpFastOpen.sendEnabled
    })
    if (!TcpFastOpen.supported) {
      tfo.setEnabled(false)
      tfo.setSummary(getString(R.string.tcp_fastopen_summary_unsupported, java.lang.System.getProperty("os.version")))
    }

    findPreference("recovery").setOnPreferenceClickListener(_ => {
      app.track("GlobalConfigFragment", "reset")
      Utils.stopSsService(getActivity)
      val h = showProgress(R.string.recovering)
      Utils.ThrowableFuture {
        app.copyAssets()
        h.sendEmptyMessage(0)
      }
      true
    })
  }

  private def showProgress(msg: Int): Handler = {
    clearDialog()
    progressDialog = ProgressDialog.show(getActivity, "", getString(msg), true, false)
    new Handler {
      override def handleMessage(msg: Message) {
        clearDialog()
      }
    }
  }

  def clearDialog() {
    if (progressDialog != null && progressDialog.isShowing) {
      if (!getActivity.isDestroyed) progressDialog.dismiss()
      progressDialog = null
    }
  }

}
