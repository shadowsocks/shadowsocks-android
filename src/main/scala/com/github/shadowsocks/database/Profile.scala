/*******************************************************************************/
/*                                                                             */
/*  Copyright (C) 2016 by Max Lv <max.c.lv@gmail.com>                          */
/*  Copyright (C) 2016 by Mygod Studio <contact-shadowsocks-android@mygod.be>  */
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

import java.net.URLEncoder
import java.util.Locale

import android.content.SharedPreferences
import android.os.Binder
import android.util.Base64
import com.github.shadowsocks.utils.Key
import com.j256.ormlite.field.{DataType, DatabaseField}

class Profile {
  @DatabaseField(generatedId = true)
  var id: Int = 0

  @DatabaseField
  var name: String = ""

  @DatabaseField
  var host: String = ""

  // hopefully hashCode = mHandle doesn't change, currently this is true from KitKat to Nougat
  @DatabaseField
  var localPort: Int = 1080 + Binder.getCallingUserHandle.hashCode

  @DatabaseField
  var remotePort: Int = 8388

  @DatabaseField
  var password: String = ""

  @DatabaseField
  var method: String = "aes-256-cfb"

  @DatabaseField
  var route: String = "all"

  @DatabaseField
  var remoteDns: String = "8.8.8.8"

  @DatabaseField
  var proxyApps: Boolean = false

  @DatabaseField
  var bypass: Boolean = false

  @DatabaseField
  var udpdns: Boolean = false

  @DatabaseField
  var auth: Boolean = false

  @DatabaseField
  var ipv6: Boolean = false

  @DatabaseField(dataType = DataType.LONG_STRING)
  var individual: String = ""

  @DatabaseField
  var tx: Long = 0

  @DatabaseField
  var rx: Long = 0

  @DatabaseField
  val date: java.util.Date = new java.util.Date()

  @DatabaseField
  var userOrder: Long = _

  @DatabaseField
  var kcp: Boolean = false

  @DatabaseField
  var kcpPort: Int = 8399

  @DatabaseField
  var kcpcli: String = "--crypt none --mode normal --mtu 1200 --nocomp --dscp 46 --parityshard 0"

  override def toString: String = "ss://" + Base64.encodeToString("%s%s:%s@%s:%d".formatLocal(Locale.ENGLISH,
    method, if (auth) "-auth" else "", password, host, remotePort).getBytes, Base64.NO_PADDING | Base64.NO_WRAP) +
    '#' + URLEncoder.encode(name, "utf-8")

  def isMethodUnsafe: Boolean = "table".equalsIgnoreCase(method) || "rc4".equalsIgnoreCase(method)

  def serialize(editor: SharedPreferences.Editor): SharedPreferences.Editor = editor
    .putString(Key.name, name)
    .putString(Key.host, host)
    .putInt(Key.localPort, localPort)
    .putInt(Key.remotePort, remotePort)
    .putString(Key.password, password)
    .putString(Key.route, route)
    .putString(Key.remoteDns, remoteDns)
    .putBoolean(Key.proxyApps, proxyApps)
    .putBoolean(Key.bypass, bypass)
    .putBoolean(Key.udpdns, udpdns)
    .putBoolean(Key.auth, auth)
    .putBoolean(Key.ipv6, ipv6)
    .putString(Key.individual, individual)
    .putBoolean(Key.kcp, kcp)
    .putInt(Key.kcpPort, kcpPort)
    .remove(Key.dirty)
  def deserialize(pref: SharedPreferences) {
    // It's assumed that default values are never used, so 0/false/null is always used even if that isn't the case
    name = pref.getString(Key.name, null)
    host = pref.getString(Key.host, null)
    localPort = pref.getInt(Key.localPort, 0)
    remotePort = pref.getInt(Key.remotePort, 0)
    password = pref.getString(Key.password, null)
    route = pref.getString(Key.route, null)
    remoteDns = pref.getString(Key.remoteDns, null)
    proxyApps = pref.getBoolean(Key.proxyApps, false)
    bypass = pref.getBoolean(Key.bypass, false)
    udpdns = pref.getBoolean(Key.udpdns, false)
    auth = pref.getBoolean(Key.auth, false)
    ipv6 = pref.getBoolean(Key.ipv6, false)
    individual = pref.getString(Key.individual, null)
    kcp = pref.getBoolean(Key.kcp, false)
    kcpPort = pref.getInt(Key.kcpPort, 0)
    kcpcli = pref.getString(Key.kcpcli, null)
  }
}
