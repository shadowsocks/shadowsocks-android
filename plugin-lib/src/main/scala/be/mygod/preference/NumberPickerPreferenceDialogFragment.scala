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

import android.content.Context
import android.support.v14.preference.PreferenceDialogFragment
import android.view.{View, ViewGroup}
import android.widget.NumberPicker

class NumberPickerPreferenceDialogFragment extends PreferenceDialogFragment {
  private lazy val preference = getPreference.asInstanceOf[NumberPickerPreference]
  private lazy val picker = preference.picker

  override protected def onCreateDialogView(context: Context): NumberPicker = {
    val parent = picker.getParent.asInstanceOf[ViewGroup]
    if (parent != null) parent.removeView(picker)
    picker
  }

  override protected def onBindDialogView(view: View) {
    super.onBindDialogView(view)
    picker.setValue(preference.getValue)
  }

  override protected def needInputMethod = true

  def onDialogClosed(positiveResult: Boolean) {
    picker.clearFocus() // commit changes
    if (positiveResult) {
      val value = picker.getValue
      if (preference.callChangeListener(value)) preference.setValue(value)
    }
  }
}
