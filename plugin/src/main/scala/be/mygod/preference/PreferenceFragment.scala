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

package be.mygod.preference

import android.app.DialogFragment
import android.os.Bundle
import android.support.v14.preference.{PreferenceFragment => Base}
import android.support.v7.preference.{Preference, PreferenceScreen}
import android.view.{LayoutInflater, View, ViewGroup}

abstract class PreferenceFragment extends Base {
  override def onCreateView(inflater: LayoutInflater, container: ViewGroup, savedInstanceState: Bundle): View =
    super.onCreateView(inflater, container, savedInstanceState)

  protected final def displayPreferenceDialog(key: String, fragment: DialogFragment, other: Bundle = null) {
    val bundle = new Bundle(1)
    bundle.putString("key", key)
    if (other != null) bundle.putAll(other)
    fragment.setArguments(bundle)
    fragment.setTargetFragment(this, 0)
    getFragmentManager.beginTransaction()
      .add(fragment, "android.support.v14.preference.PreferenceFragment.DIALOG")
      .commitAllowingStateLoss()
  }

  override def onDisplayPreferenceDialog(preference: Preference): Unit = preference match {
    case dpp: DialogPreferencePlus => displayPreferenceDialog(preference.getKey, dpp.createDialog())
    case _ => super.onDisplayPreferenceDialog(preference)
  }

  override protected def onCreateAdapter(screen: PreferenceScreen) = new PreferenceGroupAdapter(screen)

  override def onResume() {
    super.onResume()
    getListView.scrollBy(0, 0)
  }
}
