/*******************************************************************************
 *                                                                             *
 *  Copyright (C) 2018 by Max Lv <max.c.lv@gmail.com>                          *
 *  Copyright (C) 2018 by Mygod Studio <contact-shadowsocks-android@mygod.be>  *
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

import com.github.shadowsocks.core.R
import com.google.firebase.analytics.ktx.analytics
import com.google.firebase.analytics.ktx.logEvent
import com.google.firebase.ktx.Firebase
import com.google.firebase.remoteconfig.ktx.remoteConfig
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.async
import kotlinx.coroutines.tasks.await
import timber.log.Timber

object RemoteConfig {
    private val config = GlobalScope.async(Dispatchers.Main.immediate) {
        Firebase.remoteConfig.apply { setDefaultsAsync(R.xml.default_configs).await() }
    }

    fun fetchAsync() = GlobalScope.async(Dispatchers.Main.immediate) { fetch() }

    suspend fun fetch() = config.await().run {
        try {
            fetch().await()
            this to true
        } catch (e: Exception) {
            Timber.d(e)
            Firebase.analytics.logEvent("femote_config_failure") { param("message", e.toString()) }
            this to false
        }
    }
}
