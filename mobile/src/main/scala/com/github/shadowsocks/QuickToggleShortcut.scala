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
import android.support.v4.content.pm.{ShortcutInfoCompat, ShortcutManagerCompat}
import android.support.v4.graphics.drawable.IconCompat
import com.github.shadowsocks.bg.ServiceState
import com.github.shadowsocks.utils.Utils

/**
  * @author Mygod
  */
class QuickToggleShortcut extends Activity with ServiceBoundContext {
  override def onCreate(savedInstanceState: Bundle) {
    super.onCreate(savedInstanceState)
    getIntent.getAction match {
      case Intent.ACTION_CREATE_SHORTCUT =>
        setResult(Activity.RESULT_OK, ShortcutManagerCompat.createShortcutResultIntent(this,
          new ShortcutInfoCompat.Builder(this, "toggle")
            .setIntent(new Intent(this, classOf[QuickToggleShortcut]).setAction(Intent.ACTION_MAIN))
            .setIcon(IconCompat.createWithResource(this, R.drawable.ic_qu_shadowsocks_launcher))
            .setShortLabel(getString(R.string.quick_toggle))
            .build()))
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
      case ServiceState.STOPPED => Utils.startSsService(this)
      case ServiceState.CONNECTED => Utils.stopSsService(this)
      case _ => // ignore
    }
    finish()
  }
}
