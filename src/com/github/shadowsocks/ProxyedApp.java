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

package com.github.shadowsocks;

public class ProxyedApp {

  private boolean enabled;
  private int uid;
  private String username;
  private String procname;
  private String name;
  private boolean proxyed = false;

  /** @return the name */
  public String getName() {
    return name;
  }

  /** @param name the name to set */
  public void setName(String name) {
    this.name = name;
  }

  /** @return the procname */
  public String getProcname() {
    return procname;
  }

  /** @param procname the procname to set */
  public void setProcname(String procname) {
    this.procname = procname;
  }

  /** @return the uid */
  public int getUid() {
    return uid;
  }

  /** @param uid the uid to set */
  public void setUid(int uid) {
    this.uid = uid;
  }

  /** @return the username */
  public String getUsername() {
    return username;
  }

  /** @param username the username to set */
  public void setUsername(String username) {
    this.username = username;
  }

  /** @return the enabled */
  public boolean isEnabled() {
    return enabled;
  }

  /** @param enabled the enabled to set */
  public void setEnabled(boolean enabled) {
    this.enabled = enabled;
  }

  /** @return the proxyed */
  public boolean isProxyed() {
    return proxyed;
  }

  /** @param proxyed the proxyed to set */
  public void setProxyed(boolean proxyed) {
    this.proxyed = proxyed;
  }
}
