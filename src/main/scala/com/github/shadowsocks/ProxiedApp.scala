/* Shadowsocks - A shadowsocks client for Android
 * Copyright (C) 2012 <max.c.lv@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *                            ___====-_  _-====___
 *                      _--^^^#####//      \\#####^^^--_
 *                   _-^##########// (    ) \\##########^-_
 *                  -############//  |\^^/|  \\############-
 *                _/############//   (@::@)   \\############\_
 *               /#############((     \\//     ))#############\
 *              -###############\\    (oo)    //###############-
 *             -#################\\  / VV \  //#################-
 *            -###################\\/      \//###################-
 *           _#/|##########/\######(   /\   )######/\##########|\#_
 *           |/ |#/\#/\#/\/  \#/\##\  |  |  /##/\#/  \/\#/\#/\#| \|
 *           `  |/  V  V  `   V  \#\| |  | |/#/  V   '  V  V  \|  '
 *              `   `  `      `   / | |  | | \   '      '  '   '
 *                               (  | |  | |  )
 *                              __\ | |  | | /__
 *                             (vvv(VVV)(VVV)vvv)
 *
 *                              HERE BE DRAGONS
 *
 */
package com.github.shadowsocks

class ProxiedApp {
  /** @return the name */
  def getName: String = {
    name
  }

  /** @param name the name to set */
  def setName(name: String) {
    this.name = name
  }

  /** @return the procname */
  def getProcname: String = {
    procname
  }

  /** @param procname the procname to set */
  def setProcname(procname: String) {
    this.procname = procname
  }

  /** @return the uid */
  def getUid: Int = {
    uid
  }

  /** @param uid the uid to set */
  def setUid(uid: Int) {
    this.uid = uid
  }

  /** @return the username */
  def getUsername: String = {
    username
  }

  /** @param username the username to set */
  def setUsername(username: String) {
    this.username = username
  }

  /** @return the enabled */
  def isEnabled: Boolean = {
    enabled
  }

  /** @param enabled the enabled to set */
  def setEnabled(enabled: Boolean) {
    this.enabled = enabled
  }

  /** @return the proxied */
  def isProxied: Boolean = {
    proxied
  }

  /** @param proxied the proxied to set */
  def setProxyed(proxied: Boolean) {
    this.proxied = proxied
  }

  private var enabled: Boolean = false
  private var uid: Int = 0
  private var username: String = null
  private var procname: String = null
  private var name: String = null
  private var proxied: Boolean = false
}