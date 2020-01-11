/*******************************************************************************
 *                                                                             *
 *  Copyright (C) 2020 by Max Lv <max.c.lv@gmail.com>                          *
 *  Copyright (C) 2020 by Mygod Studio <contact-shadowsocks-android@mygod.be>  *
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

package com.github.shadowsocks.subscription

import androidx.recyclerview.widget.SortedList
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.utils.URLSorter
import com.github.shadowsocks.utils.asIterable
import java.io.Reader
import java.net.URL

class Subscription {
    companion object {
        const val SUBSCRIPTION = "subscription"

        var instance: Subscription
            get() {
                val sub = Subscription()
                val str = DataStore.publicStore.getString(SUBSCRIPTION)
                if (str != null) sub.fromReader(str.reader())
                return sub
            }
            set(value) = DataStore.publicStore.putString(SUBSCRIPTION, value.toString())
    }

    val urls = SortedList(URL::class.java, URLSorter)

    fun fromReader(reader: Reader): Subscription {
        urls.clear()
        reader.useLines {
            for (line in it) {
                urls.add(URL(line))
            }
        }
        return this
    }

    override fun toString(): String {
        val result = StringBuilder()
        result.append(urls.asIterable().joinToString("\n"))
        return result.toString()
    }
}
