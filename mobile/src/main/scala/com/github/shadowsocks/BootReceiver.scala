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

import android.content.pm.PackageManager
import android.content.{BroadcastReceiver, ComponentName, Context, Intent}
import com.github.shadowsocks.utils._

object BootReceiver {
  def getEnabled(context: Context): Boolean = PackageManager.COMPONENT_ENABLED_STATE_ENABLED ==
    context.getPackageManager.getComponentEnabledSetting(new ComponentName(context, classOf[BootReceiver]))
  def setEnabled(context: Context, enabled: Boolean): Unit = context.getPackageManager.setComponentEnabledSetting(
    new ComponentName(context, classOf[BootReceiver]),
    if (enabled) PackageManager.COMPONENT_ENABLED_STATE_ENABLED else PackageManager.COMPONENT_ENABLED_STATE_DISABLED,
    PackageManager.DONT_KILL_APP)
}

class BootReceiver extends BroadcastReceiver {
  def onReceive(context: Context, intent: Intent): Unit = {
    Utils.startSsService(context)
  }
}
