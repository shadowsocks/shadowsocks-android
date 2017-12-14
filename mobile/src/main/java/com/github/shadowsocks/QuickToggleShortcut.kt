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
import android.content.Intent
import android.content.pm.ShortcutManager
import android.os.Build
import android.os.Bundle
import android.support.v4.content.pm.ShortcutInfoCompat
import android.support.v4.content.pm.ShortcutManagerCompat
import android.support.v4.graphics.drawable.IconCompat
import com.github.shadowsocks.App.Companion.app
import com.github.shadowsocks.aidl.IShadowsocksService
import com.github.shadowsocks.bg.BaseService

class QuickToggleShortcut : Activity(), ShadowsocksConnection.Interface {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        if (intent.action == Intent.ACTION_CREATE_SHORTCUT) {
            setResult(Activity.RESULT_OK, ShortcutManagerCompat.createShortcutResultIntent(this,
                    ShortcutInfoCompat.Builder(this, "toggle")
                            .setIntent(Intent(this, QuickToggleShortcut::class.java).setAction(Intent.ACTION_MAIN))
                            .setIcon(IconCompat.createWithResource(this, R.drawable.ic_qu_shadowsocks_launcher))
                            .setShortLabel(getString(R.string.quick_toggle))
                            .build()))
            finish()
        } else {
            connection.connect()
            if (Build.VERSION.SDK_INT >= 25) getSystemService(ShortcutManager::class.java).reportShortcutUsed("toggle")
        }
    }

    override fun onServiceConnected(service: IShadowsocksService) {
        when (service.state) {
            BaseService.STOPPED -> app.startService()
            BaseService.CONNECTED -> app.stopService()
        }
        finish()
    }

    override fun onDestroy() {
        connection.disconnect()
        super.onDestroy()
    }
}
