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

package com.github.shadowsocks.tasker

import android.content.Context
import android.content.Intent
import android.os.Bundle
import androidx.core.os.bundleOf
import com.github.shadowsocks.R
import com.github.shadowsocks.database.ProfileManager
import com.twofortyfouram.locale.api.Intent as ApiIntent

class Settings(bundle: Bundle?) {
    companion object {
        private const val KEY_SWITCH_ON = "switch_on"
        private const val KEY_PROFILE_ID = "profile_id"

        fun fromIntent(intent: Intent) = Settings(intent.getBundleExtra(ApiIntent.EXTRA_BUNDLE))
    }

    var switchOn: Boolean = bundle?.getBoolean(KEY_SWITCH_ON, true) ?: true
    var profileId: Long

    init {
        profileId = bundle?.getLong(KEY_PROFILE_ID, -1L) ?: -1L
        if (profileId < 0) profileId = (bundle?.getInt(KEY_PROFILE_ID, -1) ?: -1).toLong()
    }

    fun toIntent(context: Context): Intent {
        val profile = ProfileManager.getProfile(profileId)
        return Intent()
                .putExtra(ApiIntent.EXTRA_BUNDLE, bundleOf(Pair(KEY_SWITCH_ON, switchOn),
                        Pair(KEY_PROFILE_ID, profileId)))
                .putExtra(ApiIntent.EXTRA_STRING_BLURB,
                        if (profile != null) context.getString(
                                if (switchOn) R.string.start_service else R.string.stop_service, profile.formattedName)
                        else context.getString(if (switchOn) R.string.start_service_default else R.string.stop))
    }
}
