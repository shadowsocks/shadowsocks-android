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

package com.github.shadowsocks.bg

import android.app.KeyguardManager
import android.content.Context
import android.graphics.drawable.Icon
import android.service.quicksettings.Tile
import android.support.annotation.RequiresApi
import com.github.shadowsocks.App.Companion.app
import com.github.shadowsocks.R
import com.github.shadowsocks.ShadowsocksConnection
import com.github.shadowsocks.aidl.IShadowsocksService
import com.github.shadowsocks.aidl.IShadowsocksServiceCallback
import android.service.quicksettings.TileService as BaseTileService

@RequiresApi(24)
class TileService : BaseTileService(), ShadowsocksConnection.Interface {
    private val iconIdle by lazy { Icon.createWithResource(this, R.drawable.ic_service_idle).setTint(0x79ffffff) }
    private val iconBusy by lazy { Icon.createWithResource(this, R.drawable.ic_service_busy) }
    private val iconConnected by lazy { Icon.createWithResource(this, R.drawable.ic_service_active) }
    private val keyguard by lazy { getSystemService(Context.KEYGUARD_SERVICE) as KeyguardManager }

    override val serviceCallback: IShadowsocksServiceCallback.Stub by lazy {
        @RequiresApi(24)
        object : IShadowsocksServiceCallback.Stub() {
            override fun stateChanged(state: Int, profileName: String?, msg: String?) {
                val tile = qsTile ?: return
                var label: String? = null
                when (state) {
                    BaseService.STOPPED -> {
                        tile.icon = iconIdle
                        tile.state = Tile.STATE_INACTIVE
                    }
                    BaseService.CONNECTED -> {
                        tile.icon = iconConnected
                        if (!keyguard.isDeviceLocked) label = profileName
                        tile.state = Tile.STATE_ACTIVE
                    }
                    else -> {
                        tile.icon = iconBusy
                        tile.state = Tile.STATE_UNAVAILABLE
                    }
                }
                tile.label = label ?: getString(R.string.app_name)
                tile.updateTile()
            }
            override fun trafficUpdated(profileId: Int, txRate: Long, rxRate: Long, txTotal: Long, rxTotal: Long) { }
            override fun trafficPersisted(profileId: Int) { }
        }
    }

    override fun onServiceConnected(service: IShadowsocksService) =
            serviceCallback.stateChanged(service.state, service.profileName, null)

    override fun onStartListening() {
        super.onStartListening()
        connection.connect()
    }
    override fun onStopListening() {
        super.onStopListening()
        connection.disconnect()
    }

    override fun onClick() {
        if (isLocked) unlockAndRun(this::toggle) else toggle()
    }

    private fun toggle() {
        val service = connection.service ?: return
        when (service.state) {
            BaseService.STOPPED -> app.startService()
            BaseService.CONNECTED -> app.stopService()
        }
    }
}
