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
import android.content.res.TypedArray
import android.support.v7.preference.DialogPreference
import android.util.AttributeSet
import android.view.ContextThemeWrapper
import android.widget.NumberPicker
import com.github.shadowsocks.plugin.R

class NumberPickerPreference(private val context: Context, attrs: AttributeSet = null)
  extends DialogPreference(context, attrs) with DialogPreferencePlus with SummaryPreference {
  private[preference] val picker = new NumberPicker(new ContextThemeWrapper(context, R.style.NumberPickerStyle))
  private var value: Int = _

  {
    val a: TypedArray = context.obtainStyledAttributes(attrs, R.styleable.NumberPickerPreference)
    setMin(a.getInt(R.styleable.NumberPickerPreference_min, 0))
    setMax(a.getInt(R.styleable.NumberPickerPreference_max, Int.MaxValue - 1))
    a.recycle()
  }

  override def createDialog() = new NumberPickerPreferenceDialogFragment()

  def getValue: Int = value
  def getMin: Int = if (picker == null) 0 else picker.getMinValue
  def getMax: Int = picker.getMaxValue
  def setValue(i: Int) {
    if (i == value) return
    picker.setValue(i)
    value = picker.getValue
    persistInt(value)
    notifyChanged()
  }
  def setMin(value: Int): Unit = picker.setMinValue(value)
  def setMax(value: Int): Unit = picker.setMaxValue(value)

  override protected def onGetDefaultValue(a: TypedArray, index: Int): AnyRef =
    a.getInt(index, getMin).asInstanceOf[AnyRef]
  override protected def onSetInitialValue(restorePersistedValue: Boolean, defaultValue: Any) {
    val default = defaultValue.asInstanceOf[Int]
    setValue(if (restorePersistedValue) getPersistedInt(default) else default)
  }
  protected def getSummaryValue: AnyRef = getValue.asInstanceOf[AnyRef]
}
