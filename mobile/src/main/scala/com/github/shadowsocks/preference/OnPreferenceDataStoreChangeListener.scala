package com.github.shadowsocks.preference

import android.support.v7.preference.PreferenceDataStore

/**
  * @author Mygod
  */
trait OnPreferenceDataStoreChangeListener {
  def onPreferenceDataStoreChanged(store: PreferenceDataStore, key: String)
}
