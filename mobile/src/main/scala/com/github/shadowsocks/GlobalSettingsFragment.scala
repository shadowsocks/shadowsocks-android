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

import android.os.Bundle
import android.view.{LayoutInflater, View, ViewGroup}
import android.app.Fragment

class GlobalSettingsFragment extends ToolbarFragment {

  override def onCreateView(inflater: LayoutInflater, container: ViewGroup, savedInstanceState: Bundle): View =
    inflater.inflate(R.layout.layout_global_settings, container, false)

  override def onViewCreated(view: View, savedInstanceState: Bundle) {
    super.onViewCreated(view, savedInstanceState)
    toolbar.setTitle(R.string.settings)

    val fm = getChildFragmentManager
    fm.beginTransaction().replace(R.id.content, new GlobalConfigFragment()).commit()
    fm.executePendingTransactions()
  }

  override def onDetach() {
    super.onDetach()

    try {
      val childFragmentManager = classOf[Fragment].getDeclaredField("mChildFragmentManager")
      childFragmentManager.setAccessible(true)
      childFragmentManager.set(this, null)
    } catch {
      case _: Exception =>  // ignore
    }
  }
}
