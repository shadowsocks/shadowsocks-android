/*******************************************************************************
 *                                                                             *
 *  Copyright (C) 2017 by Max Lv <max.c.lv@gmail.com>                          *
 *  Copyright (C) 2017 by Mygod Studio <contact-shadowsocks-android@mygod.be>  *
 *                                                                             *
 *  This program is free software: you can redistribute it and/or modify       *
 *  it under the terms of the GNU General Public License as published by       *
 *  the Free Software Foundation, either version 3 of the License, or          *
 *  (at your option) any later version.                                        *
 *                                                                             *
 *  This program is distributed in the hope that it will be useful,            *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 *  GNU General Public License for more details.                               *
 *                                                                             *
 *  You should have received a copy of the GNU General Public License          *
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.       *
 *                                                                             *
 *******************************************************************************/

package com.github.shadowsocks.preference

import android.os.Binder
import android.support.v7.preference.PreferenceDataStore
import com.github.shadowsocks.database.DBHelper
import com.github.shadowsocks.database.KeyValuePair
import com.github.shadowsocks.utils.Key
import com.github.shadowsocks.utils.parsePort
import java.util.*

object DataStore : PreferenceDataStore() {
    fun getBoolean(key: String?) = DBHelper.kvPairDao.queryForId(key)?.boolean
    fun getFloat(key: String?) = DBHelper.kvPairDao.queryForId(key)?.float
    fun getInt(key: String?) = DBHelper.kvPairDao.queryForId(key)?.int
    fun getLong(key: String?) = DBHelper.kvPairDao.queryForId(key)?.long
    fun getString(key: String?) = DBHelper.kvPairDao.queryForId(key)?.string
    fun getStringSet(key: String?) = DBHelper.kvPairDao.queryForId(key)?.stringSet

    override fun getBoolean(key: String?, defValue: Boolean) = getBoolean(key) ?: defValue
    override fun getFloat(key: String?, defValue: Float) = getFloat(key) ?: defValue
    override fun getInt(key: String?, defValue: Int) = getInt(key) ?: defValue
    override fun getLong(key: String?, defValue: Long) = getLong(key) ?: defValue
    override fun getString(key: String?, defValue: String?) = getString(key) ?: defValue
    override fun getStringSet(key: String?, defValue: MutableSet<String>?) = getStringSet(key) ?: defValue

    fun putBoolean(key: String?, value: Boolean?) = if (value == null) remove(key) else putBoolean(key, value)
    fun putFloat(key: String?, value: Float?) = if (value == null) remove(key) else putFloat(key, value)
    fun putInt(key: String?, value: Int?) = if (value == null) remove(key) else putInt(key, value)
    fun putLong(key: String?, value: Long?) = if (value == null) remove(key) else putLong(key, value)
    override fun putBoolean(key: String?, value: Boolean) {
        DBHelper.kvPairDao.createOrUpdate(KeyValuePair(key).put(value))
        fireChangeListener(key)
    }
    override fun putFloat(key: String?, value: Float) {
        DBHelper.kvPairDao.createOrUpdate(KeyValuePair(key).put(value))
        fireChangeListener(key)
    }
    override fun putInt(key: String?, value: Int) {
        DBHelper.kvPairDao.createOrUpdate(KeyValuePair(key).put(value))
        fireChangeListener(key)
    }
    override fun putLong(key: String?, value: Long) {
        DBHelper.kvPairDao.createOrUpdate(KeyValuePair(key).put(value))
        fireChangeListener(key)
    }
    override fun putString(key: String?, value: String?) = if (value == null) remove(key) else {
        DBHelper.kvPairDao.createOrUpdate(KeyValuePair(key).put(value))
        fireChangeListener(key)
    }
    override fun putStringSet(key: String?, values: MutableSet<String>?) = if (values == null) remove(key) else {
        DBHelper.kvPairDao.createOrUpdate(KeyValuePair(key).put(values))
        fireChangeListener(key)
    }

    fun remove(key: String?) {
        DBHelper.kvPairDao.deleteById(key)
        fireChangeListener(key)
    }

    private val listeners = HashSet<OnPreferenceDataStoreChangeListener>()
    private fun fireChangeListener(key: String?) = listeners.forEach { it.onPreferenceDataStoreChanged(this, key) }
    fun registerChangeListener(listener: OnPreferenceDataStoreChangeListener) = listeners.add(listener)
    fun unregisterChangeListener(listener: OnPreferenceDataStoreChangeListener) = listeners.remove(listener)

    // hopefully hashCode = mHandle doesn't change, currently this is true from KitKat to Nougat
    private val userIndex by lazy { Binder.getCallingUserHandle().hashCode() }
    private fun getLocalPort(key: String, default: Int): Int {
        val value = getInt(key)
        return if (value != null) {
            putString(key, value.toString())
            value
        } else parsePort(getString(key), default + userIndex)
    }

    var profileId: Int
        get() = getInt(Key.id) ?: 0
        set(value) = putInt(Key.id, value)
    var serviceMode: String
        get() = getString(Key.serviceMode) ?: Key.modeVpn
        set(value) = putString(Key.serviceMode, value)
    var portProxy: Int
        get() = getLocalPort(Key.portProxy, 1080)
        set(value) = putString(Key.portProxy, value.toString())
    var portLocalDns: Int
        get() = getLocalPort(Key.portLocalDns, 5450)
        set(value) = putString(Key.portLocalDns, value.toString())
    var portTransproxy: Int
        get() = getLocalPort(Key.portTransproxy, 8200)
        set(value) = putString(Key.portTransproxy, value.toString())

    var proxyApps: Boolean
        get() = getBoolean(Key.proxyApps) ?: false
        set(value) = putBoolean(Key.proxyApps, value)
    var bypass: Boolean
        get() = getBoolean(Key.bypass) ?: false
        set(value) = putBoolean(Key.bypass, value)
    var individual: String
        get() = getString(Key.individual) ?: ""
        set(value) = putString(Key.individual, value)
    var plugin: String
        get() = getString(Key.plugin) ?: ""
        set(value) = putString(Key.plugin, value)
    var dirty: Boolean
        get() = getBoolean(Key.dirty) ?: false
        set(value) = putBoolean(Key.dirty, value)

    fun initGlobal() {
        // temporary workaround for support lib bug
        if (getString(Key.serviceMode) == null) serviceMode = serviceMode
        if (getString(Key.portProxy) == null) portProxy = portProxy
        if (getString(Key.portLocalDns) == null) portLocalDns = portLocalDns
        if (getString(Key.portTransproxy) == null) portTransproxy = portTransproxy
    }
}
