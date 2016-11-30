/*******************************************************************************/
/*                                                                             */
/*  Copyright (C) 2016 by Max Lv <max.c.lv@gmail.com>                          */
/*  Copyright (C) 2016 by Mygod Studio <contact-shadowsocks-android@mygod.be>  */
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

class EditTextPreferenceDialogFragment extends PreferenceDialogFragment {
  private lazy val preference = getPreference.asInstanceOf[EditTextPreference]
  private lazy val editText = preference.editText

  override protected def onCreateDialogView(context: Context) = {
    val parent = editText.getParent.asInstanceOf[ViewGroup]
    if (parent != null) parent.removeView(editText)
    editText
  }

  override protected def onBindDialogView(view: View) {
    super.onBindDialogView(view)
    editText.setText(preference.getText)
    val text = editText.getText
    if (text != null) editText.setSelection(0, text.length)
  }

  override protected def needInputMethod = true

  def onDialogClosed(positiveResult: Boolean) = if (positiveResult) {
    val value = editText.getText.toString
    if (preference.callChangeListener(value)) preference.setText(value)
  }
}
