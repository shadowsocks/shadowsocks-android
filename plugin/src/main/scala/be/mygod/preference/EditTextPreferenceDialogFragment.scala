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

import android.support.v14.preference.PreferenceDialogFragment
import android.support.v7.widget.AppCompatEditText
import android.view.{View, ViewGroup}

class EditTextPreferenceDialogFragment extends PreferenceDialogFragment {
  private lazy val preference = getPreference.asInstanceOf[EditTextPreference]
  protected lazy val editText: AppCompatEditText = preference.editText

  override protected def onBindDialogView(view: View) {
    super.onBindDialogView(view)
    editText.setText(preference.getText)
    val text = editText.getText
    if (text != null) editText.setSelection(0, text.length)
    val oldParent = editText.getParent.asInstanceOf[ViewGroup]
    if (oldParent eq view) return
    if (oldParent != null) oldParent.removeView(editText)
    val oldEdit = view.findViewById[View](android.R.id.edit)
    if (oldEdit == null) return
    val container = oldEdit.getParent.asInstanceOf[ViewGroup]
    if (container == null) return
    container.removeView(oldEdit)
    container.addView(editText, ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
  }

  override protected def needInputMethod = true

  def onDialogClosed(positiveResult: Boolean): Unit = if (positiveResult) {
    val value = editText.getText.toString
    if (preference.callChangeListener(value)) preference.setText(value)
  }
}
