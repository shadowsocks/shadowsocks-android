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

import android.app.Service
import android.content.Intent
import android.net.VpnService
import android.os.{Handler, IBinder}
import com.github.shadowsocks.ShadowsocksApplication.app

class ShadowsocksRunnerService extends Service with ServiceBoundContext {
  val handler = new Handler()

  override def onBind(intent: Intent): IBinder = {
    null
  }

  override def onServiceConnected() {
    handler.postDelayed(() => {
      if (bgService != null) {
        if (app.isNatEnabled) startBackgroundService()
        else if (VpnService.prepare(ShadowsocksRunnerService.this) == null) startBackgroundService()
        handler.postDelayed(() => stopSelf(), 3000)
      }
    }, 1000)
  }

  def startBackgroundService(): Unit = bgService.useSync(app.profileId)

  override def onCreate() {
    super.onCreate()
    attachService()
  }

  override def onDestroy() {
    super.onDestroy()
    detachService()
  }
}
