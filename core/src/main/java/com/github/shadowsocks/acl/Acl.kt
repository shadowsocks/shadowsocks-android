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

package com.github.shadowsocks.acl

import android.content.Context
import android.util.Log
import androidx.recyclerview.widget.SortedList
import com.github.shadowsocks.Core
import com.github.shadowsocks.net.Subnet
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.utils.asIterable
import java.io.File
import java.io.IOException
import java.io.Reader
import java.net.URL
import java.net.URLConnection

class Acl {
    companion object {
        const val TAG = "Acl"
        const val ALL = "all"
        const val BYPASS_LAN = "bypass-lan"
        const val BYPASS_CHN = "bypass-china"
        const val BYPASS_LAN_CHN = "bypass-lan-china"
        const val GFWLIST = "gfwlist"
        const val CHINALIST = "china-list"
        const val CUSTOM_RULES = "custom-rules"

        fun getFile(id: String, context: Context = Core.deviceStorage) = File(context.noBackupFilesDir, "$id.acl")
    }
}
