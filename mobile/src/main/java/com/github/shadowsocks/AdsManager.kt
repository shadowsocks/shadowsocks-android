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

package com.github.shadowsocks

import android.content.Context
import com.google.android.gms.ads.AdLoader
import com.google.android.gms.ads.AdRequest
import com.google.android.gms.ads.MobileAds
import com.google.android.gms.ads.RequestConfiguration

internal object AdsManager {
    init {
        MobileAds.setRequestConfiguration(RequestConfiguration.Builder().apply {
            setTestDeviceIds(listOf(
                    "B08FC1764A7B250E91EA9D0D5EBEB208", "7509D18EB8AF82F915874FEF53877A64",
                    "F58907F28184A828DD0DB6F8E38189C6", "FE983F496D7C5C1878AA163D9420CA97"))
        }.build())
    }

    fun load(context: Context?, setup: AdLoader.Builder.() -> Unit) =
            AdLoader.Builder(context, "ca-app-pub-3283768469187309/8632513739").apply(setup).build()
                    .loadAd(AdRequest.Builder().build())
}
