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

package com.github.shadowsocks.database

import java.util.Locale

import android.net.Uri
import android.util.Base64
import com.github.shadowsocks.plugin.PluginConfiguration
import com.github.shadowsocks.preference.OrmLitePreferenceDataStore
import com.github.shadowsocks.utils.{Key, Utils}
import com.j256.ormlite.field.{DataType, DatabaseField}

class Profile {
  @DatabaseField(generatedId = true)
  var id: Int = _

  @DatabaseField
  var name: String = ""

  @DatabaseField
  var host: String = "198.199.101.152"

  @DatabaseField
  var remotePort: Int = 8388

  @DatabaseField
  var password: String = "u1rRWTssNv0p"

  @DatabaseField
  var method: String = "aes-256-cfb"

  @DatabaseField
  var route: String = "all"

  @DatabaseField
  var remoteDns: String = "8.8.8.8"

  @DatabaseField
  var proxyApps: Boolean = _

  @DatabaseField
  var bypass: Boolean = _

  @DatabaseField
  var udpdns: Boolean = _

  @DatabaseField
  var ipv6: Boolean = _

  @DatabaseField(dataType = DataType.LONG_STRING)
  var individual: String = ""

  @DatabaseField
  var tx: Long = _

  @DatabaseField
  var rx: Long = _

  @DatabaseField
  val date: java.util.Date = new java.util.Date()

  @DatabaseField
  var userOrder: Long = _

  @DatabaseField
  var plugin: String = _

  def formattedAddress: String = (if (host.contains(":")) "[%s]:%d" else "%s:%d").format(host, remotePort)
  def nameIsEmpty: Boolean = name == null || name.isEmpty
  def getName: String = if (nameIsEmpty) formattedAddress else name

  def toUri: Uri = {
    val builder = new Uri.Builder()
      .scheme("ss")
      .encodedAuthority("%s@%s:%d".formatLocal(Locale.ENGLISH,
        Base64.encodeToString("%s:%s".formatLocal(Locale.ENGLISH, method, password).getBytes,
          Base64.NO_PADDING | Base64.NO_WRAP | Base64.URL_SAFE),
        if (host.contains(':')) '[' + host + ']' else host, remotePort))
    val configuration = new PluginConfiguration(plugin)
    if (configuration.selected.nonEmpty)
      builder.appendQueryParameter(Key.plugin, configuration.selectedOptions.toString(false))
    if (!nameIsEmpty) builder.fragment(name)
    builder.build()
  }
  override def toString: String = toUri.toString

  def serialize(store: OrmLitePreferenceDataStore) {
    store.putString(Key.name, name)
    store.putString(Key.host, host)
    store.putString(Key.remotePort, remotePort.toString)
    store.putString(Key.password, password)
    store.putString(Key.route, route)
    store.putString(Key.remoteDns, remoteDns)
    store.putString(Key.method, method)
    store.proxyApps = proxyApps
    store.bypass = bypass
    store.putBoolean(Key.udpdns, udpdns)
    store.putBoolean(Key.ipv6, ipv6)
    store.individual = individual
    store.plugin = plugin
    store.remove(Key.dirty)
  }
  def deserialize(store: OrmLitePreferenceDataStore) {
    // It's assumed that default values are never used, so 0/false/null is always used even if that isn't the case
    name = store.getString(Key.name, null)
    host = store.getString(Key.host, null)
    remotePort = Utils.parsePort(store.getString(Key.remotePort), 8388, 1)
    password = store.getString(Key.password, null)
    method = store.getString(Key.method, null)
    route = store.getString(Key.route, null)
    remoteDns = store.getString(Key.remoteDns, null)
    proxyApps = store.proxyApps
    bypass = store.bypass
    udpdns = store.getBoolean(Key.udpdns, false)
    ipv6 = store.getBoolean(Key.ipv6, false)
    individual = store.individual
    plugin = store.plugin
  }
}
