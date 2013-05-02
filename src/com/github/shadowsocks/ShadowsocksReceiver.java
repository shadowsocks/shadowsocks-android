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

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.preference.PreferenceManager;

public class ShadowsocksReceiver extends BroadcastReceiver {

  public static final String CLOSE_ACTION = "com.github.shadowsocks.ACTION_SHUTDOWN";

  private static final String TAG = "Shadowsocks";

  @Override
  public void onReceive(Context context, Intent intent) {

    if (intent != null) {
      final String action = intent.getAction();
      if (action.equals(Intent.ACTION_SHUTDOWN)
          || action.equals(CLOSE_ACTION)) {
        context.stopService(new Intent(context, ShadowsocksService.class));
        return;
      }
    }

    SharedPreferences settings = PreferenceManager.getDefaultSharedPreferences(context);
    String versionName;
    try {
      versionName =
          context.getPackageManager().getPackageInfo(context.getPackageName(), 0).versionName;
    } catch (PackageManager.NameNotFoundException e) {
      versionName = "NONE";
    }

    final boolean isAutoConnect = settings.getBoolean("isAutoConnect", false);
    final boolean isInstalled = settings.getBoolean(versionName, false);

    if (isAutoConnect && isInstalled) {
      final String portText = settings.getString("port", "");
      if (portText == null || portText.length() <= 0) {
        return;
      }
      try {
        int port = Integer.valueOf(portText);
        if (port <= 1024) {
          return;
        }
      } catch (Exception e) {
        return;
      }

      Intent it = new Intent(context, ShadowsocksService.class);
      context.startService(it);
    }
  }
}
