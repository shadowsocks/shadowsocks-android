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

package com.github.shadowsocks

import android.app.Activity
import android.content.Intent
import android.content.pm.ShortcutManager
import android.os.{Build, Bundle}
import com.github.shadowsocks.utils.{State, Utils}

/**
  * @author Mygod
  */
class QuickToggleShortcut extends Activity with ServiceBoundContext {
  override def onCreate(savedInstanceState: Bundle) {
    super.onCreate(savedInstanceState)
    getIntent.getAction match {
      case Intent.ACTION_CREATE_SHORTCUT =>
        setResult(Activity.RESULT_OK, new Intent()
          .putExtra(Intent.EXTRA_SHORTCUT_INTENT, new Intent(this, classOf[QuickToggleShortcut]))
          .putExtra(Intent.EXTRA_SHORTCUT_NAME, getString(R.string.quick_toggle))
          .putExtra(Intent.EXTRA_SHORTCUT_ICON_RESOURCE,
            Intent.ShortcutIconResource.fromContext(this, R.mipmap.ic_launcher)))
        finish()
      case _ =>
        attachService()
        if (Build.VERSION.SDK_INT >= 25) getSystemService(classOf[ShortcutManager]).reportShortcutUsed("toggle")
    }
  }

  override def onDestroy() {
    detachService()
    super.onDestroy()
  }

  override def onServiceConnected() {
    bgService.getState match {
      case State.STOPPED => Utils.startSsService(this)
      case State.CONNECTED => Utils.stopSsService(this)
      case _ => // ignore
    }
    finish()
  }
}
