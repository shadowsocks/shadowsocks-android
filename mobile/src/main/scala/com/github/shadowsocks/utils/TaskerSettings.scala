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

package com.github.shadowsocks.utils

import android.content.{Context, Intent}
import android.os.Bundle
import com.github.shadowsocks.R
import com.github.shadowsocks.ShadowsocksApplication.app
import com.twofortyfouram.locale.api.{Intent => ApiIntent}

object TaskerSettings {
  private val KEY_SWITCH_ON = "switch_on"
  private val KEY_PROFILE_ID = "profile_id"

  def fromIntent(intent: Intent) = new TaskerSettings(intent.getBundleExtra(ApiIntent.EXTRA_BUNDLE))
}

class TaskerSettings(bundle: Bundle) {
  import TaskerSettings._

  var switchOn: Boolean = bundle.getBoolean(KEY_SWITCH_ON, true)
  var profileId: Int = bundle.getInt(KEY_PROFILE_ID, -1)

  def toIntent(context: Context): Intent = {
    val bundle = new Bundle()
    if (!switchOn) bundle.putBoolean(KEY_SWITCH_ON, false)
    if (profileId >= 0) bundle.putInt(KEY_PROFILE_ID, profileId)
    new Intent().putExtra(ApiIntent.EXTRA_BUNDLE, bundle).putExtra(ApiIntent.EXTRA_STRING_BLURB,
      app.profileManager.getProfile(profileId) match {
        case Some(p) => context.getString(if (switchOn) R.string.start_service else R.string.stop_service, p.getName)
        case None => context.getString(if (switchOn) R.string.start_service_default else R.string.stop)
      })
  }
}

