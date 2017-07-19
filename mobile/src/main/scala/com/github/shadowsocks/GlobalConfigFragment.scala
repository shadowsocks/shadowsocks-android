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
import android.support.v7.preference.PreferenceDataStore
import be.mygod.preference.PreferenceFragment
import com.github.shadowsocks.ShadowsocksApplication.app
import com.github.shadowsocks.preference.OnPreferenceDataStoreChangeListener
import com.github.shadowsocks.utils.{Key, TcpFastOpen}

class GlobalConfigFragment extends PreferenceFragment with OnPreferenceDataStoreChangeListener {
  override def onCreatePreferences(bundle: Bundle, key: String) {
    getPreferenceManager.setPreferenceDataStore(app.dataStore)
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
    app.dataStore.registerChangeListener(this)
  }

  override def onDestroy() {
    app.dataStore.unregisterChangeListener(this)
    super.onDestroy()
  }

  def onPreferenceDataStoreChanged(store: PreferenceDataStore, key: String): Unit = key match {
    case Key.isNAT => findPreference(key).asInstanceOf[SwitchPreference].setChecked(store.getBoolean(key, false))
    case _ =>
  }
}
