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
import android.support.v7.preference.{EditTextPreference => Parent}
import android.support.v7.widget.AppCompatEditText
import android.text.InputType
import android.util.AttributeSet
import android.view.ViewGroup
import android.widget.FrameLayout
import com.github.shadowsocks.plugin.R

/**
  * Fixed EditTextPreference + SummaryPreference with password support!
  * Based on: https://github.com/Gericop/Android-Support-Preference-V7-Fix/tree/master/app/src/main/java/android/support/v7/preference
  */
class EditTextPreference(context: Context, attrs: AttributeSet = null) extends Parent(context, attrs)
  with DialogPreferencePlus with SummaryPreference {
  val editText = new AppCompatEditText(context, attrs)
  editText.setId(android.R.id.edit)

  {
    val arr = context.obtainStyledAttributes(Array(R.attr.dialogPreferredPadding))
    val margin = arr.getDimensionPixelOffset(0, 0)
    arr.recycle()
    val params = new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
    params.setMargins(margin, 0, margin, 0)
    editText.setLayoutParams(params)
  }

  override def createDialog() = new EditTextPreferenceDialogFragment()

  override protected def getSummaryValue: String = {
    var text = getText
    if (text == null) text = ""
    val inputType = editText.getInputType
    if (inputType == (InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD) ||
      inputType == (InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_WEB_PASSWORD) ||
      inputType == (InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_VARIATION_PASSWORD))
      "\u2022" * text.length else text
  }

  override def setText(text: String): Unit = {
    val old = getText
    super.setText(text)
    if (old != text) notifyChanged()
  }
}
