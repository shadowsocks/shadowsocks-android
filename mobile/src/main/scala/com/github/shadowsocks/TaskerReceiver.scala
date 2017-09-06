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

import android.content.{BroadcastReceiver, Context, Intent}
import com.github.shadowsocks.utils.{TaskerSettings, Utils}
import com.github.shadowsocks.ShadowsocksApplication.app

/**
  * @author CzBiX
  */
class TaskerReceiver extends BroadcastReceiver {
  override def onReceive(context: Context, intent: Intent) {
    val settings = TaskerSettings.fromIntent(intent)
    var changed = false
    app.profileManager.getProfile(settings.profileId) match {
      case Some(_) => if (app.dataStore.profileId != settings.profileId) {
        app.switchProfile(settings.profileId)
        changed = true
      }
      case _ =>
    }
    if (settings.switchOn) {
      Utils.startSsService(context)
      if (changed) Utils.reloadSsService(context)
    } else Utils.stopSsService(context)
  }
}
