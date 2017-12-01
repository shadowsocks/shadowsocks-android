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

import android.os.Bundle
import android.support.design.widget.Snackbar
import android.support.v14.preference.SwitchPreference
import android.support.v7.preference.Preference
import com.github.shadowsocks.ShadowsocksApplication.app
import com.github.shadowsocks.bg.ServiceState
import com.github.shadowsocks.utils.{Key, TcpFastOpen}
import com.takisoft.fix.support.v7.preference.PreferenceFragmentCompatDividers

class GlobalConfigFragment extends PreferenceFragmentCompatDividers {
  override def onCreatePreferencesFix(bundle: Bundle, key: String) {
    getPreferenceManager.setPreferenceDataStore(app.dataStore)
    app.dataStore.initGlobal()
    addPreferencesFromResource(R.xml.pref_global)
    val switch = findPreference(Key.isAutoConnect).asInstanceOf[SwitchPreference]
    switch.setOnPreferenceChangeListener((_, value) => {
      BootReceiver.setEnabled(getActivity, value.asInstanceOf[Boolean])
      true
    })
    switch.setChecked(BootReceiver.getEnabled(getActivity))

    val tfo = findPreference(Key.tfo).asInstanceOf[SwitchPreference]
    tfo.setChecked(TcpFastOpen.sendEnabled)
    tfo.setOnPreferenceChangeListener((_, v) => {
      val value = v.asInstanceOf[Boolean]
      val result = TcpFastOpen.enabled(value)
      if (result != null && result != "Success.")
        Snackbar.make(getActivity.findViewById(R.id.snackbar), result, Snackbar.LENGTH_LONG).show()
      value == TcpFastOpen.sendEnabled
    })
    if (!TcpFastOpen.supported) {
      tfo.setEnabled(false)
      tfo.setSummary(getString(R.string.tcp_fastopen_summary_unsupported, java.lang.System.getProperty("os.version")))
    }

    val serviceMode = findPreference(Key.serviceMode)
    val portProxy = findPreference(Key.portProxy)
    val portLocalDns = findPreference(Key.portLocalDns)
    val portTransproxy = findPreference(Key.portTransproxy)
    def onServiceModeChange(p: Preference, v: Any) = {
      val (enabledLocalDns, enabledTransproxy) = v match {
        case Key.modeProxy => (false, false)
        case Key.modeVpn => (true, false)
        case Key.modeTransproxy => (true, true)
      }
      portLocalDns.setEnabled(enabledLocalDns)
      portTransproxy.setEnabled(enabledTransproxy)
      true
    }
    MainActivity.stateListener = {
      case ServiceState.IDLE | ServiceState.STOPPED =>
        serviceMode.setEnabled(true)
        portProxy.setEnabled(true)
        onServiceModeChange(null, app.dataStore.serviceMode)
      case _ =>
        serviceMode.setEnabled(false)
        portProxy.setEnabled(false)
        portLocalDns.setEnabled(false)
        portTransproxy.setEnabled(false)
    }
    MainActivity.stateListener(getActivity.asInstanceOf[MainActivity].state)
    serviceMode.setOnPreferenceChangeListener(onServiceModeChange)
  }

  override def onDestroy() {
    MainActivity.stateListener = null
    super.onDestroy()
  }
}
