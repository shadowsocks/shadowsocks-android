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

package com.github.shadowsocks

import android.app.Activity
import android.app.KeyguardManager
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.net.VpnService
import android.os.Bundle
import android.util.Log
import com.github.shadowsocks.App.Companion.app
import com.github.shadowsocks.aidl.IShadowsocksService
import com.github.shadowsocks.bg.BaseService
import com.github.shadowsocks.utils.broadcastReceiver

class VpnRequestActivity : Activity(), ShadowsocksConnection.Interface {
    companion object {
        private const val TAG = "VpnRequestActivity"
        private const val REQUEST_CONNECT = 1
    }

    private var receiver: BroadcastReceiver? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        if (!BaseService.usingVpnMode) {
            finish()
            return
        }
        val km = getSystemService(Context.KEYGUARD_SERVICE) as KeyguardManager
        if (km.inKeyguardRestrictedInputMode()) {
            receiver = broadcastReceiver { _, _ -> connection.connect() }
            registerReceiver(receiver, IntentFilter(Intent.ACTION_USER_PRESENT))
        } else connection.connect()
    }

    override fun onServiceConnected(service: IShadowsocksService) {
        app.handler.postDelayed({
            val intent = VpnService.prepare(this)
            if (intent == null) onActivityResult(REQUEST_CONNECT, RESULT_OK, null)
            else startActivityForResult(intent, REQUEST_CONNECT)
        }, 1000)
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        if (resultCode == RESULT_OK) app.startService() else Log.e(TAG, "Failed to start VpnService")
        finish()
    }

    override fun onDestroy() {
        super.onDestroy()
        connection.disconnect()
        if (receiver != null) unregisterReceiver(receiver)
    }
}
