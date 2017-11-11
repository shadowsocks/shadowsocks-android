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

package com.github.shadowsocks.bg

import android.annotation.TargetApi
import android.graphics.drawable.Icon
import android.service.quicksettings.{Tile, TileService}
import com.github.shadowsocks.aidl.IShadowsocksServiceCallback
import com.github.shadowsocks.utils.Utils
import com.github.shadowsocks.{R, ServiceBoundContext}

/**
  * @author Mygod
  */
@TargetApi(24)
final class ServiceTileService extends TileService with ServiceBoundContext {
  private lazy val iconIdle = Icon.createWithResource(this, R.drawable.ic_start_idle).setTint(0x80ffffff)
  private lazy val iconBusy = Icon.createWithResource(this, R.drawable.ic_start_busy)
  private lazy val iconConnected = Icon.createWithResource(this, R.drawable.ic_start_connected)
  private lazy val callback = new IShadowsocksServiceCallback.Stub {
    def trafficUpdated(profileId: Int, txRate: Long, rxRate: Long, txTotal: Long, rxTotal: Long): Unit = ()
    def stateChanged(state: Int, profileName: String, msg: String) {
      val tile = getQsTile
      if (tile != null) {
        state match {
          case ServiceState.STOPPED =>
            tile.setIcon(iconIdle)
            tile.setLabel(getString(R.string.app_name))
            tile.setState(Tile.STATE_INACTIVE)
          case ServiceState.CONNECTED =>
            tile.setIcon(iconConnected)
            tile.setLabel(if (profileName == null) getString(R.string.app_name) else profileName)
            tile.setState(Tile.STATE_ACTIVE)
          case _ =>
            tile.setIcon(iconBusy)
            tile.setLabel(getString(R.string.app_name))
            tile.setState(Tile.STATE_UNAVAILABLE)
        }
        tile.updateTile()
      }
    }
    override def trafficPersisted(profileId: Int): Unit = ()
  }

  override def onServiceConnected(): Unit = callback.stateChanged(bgService.getState, bgService.getProfileName, null)

  override def onStartListening() {
    super.onStartListening()
    attachService(callback)
  }
  override def onStopListening() {
    super.onStopListening()
    detachService() // just in case the user switches to NAT mode, also saves battery
  }

  override def onClick(): Unit = if (isLocked) unlockAndRun(toggle) else toggle()

  private def toggle() = if (bgService != null) bgService.getState match {
    case ServiceState.STOPPED => Utils.startSsService(this)
    case ServiceState.CONNECTED => Utils.stopSsService(this)
    case _ => // ignore
  }
}
