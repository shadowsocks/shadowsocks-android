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

import java.lang.reflect.Field
import java.util

import android.os.Build
import android.support.v4.content.ContextCompat
import android.support.v4.view.ViewCompat
import android.support.v7.preference.{PreferenceGroup, PreferenceViewHolder, PreferenceGroupAdapter => Old}
import android.view.{LayoutInflater, View, ViewGroup}
import com.github.shadowsocks.plugin.R

/**
  * Fix by: https://github.com/Gericop/Android-Support-Preference-V7-Fix/commit/7de016b007e28a264001a8bb353f110a7f64bb69
  *
  * @author Mygod
  */
object PreferenceGroupAdapter {
  private var preferenceLayoutsField: Field = _
  private var fieldResId: Field = _
  private var fieldWidgetResId: Field = _
  private val preferenceViewHolderConstructor = classOf[PreferenceViewHolder].getDeclaredConstructor(classOf[View])

  {
    val oldClass = classOf[Old]
    preferenceLayoutsField = oldClass.getDeclaredField("mPreferenceLayouts")
    preferenceLayoutsField.setAccessible(true)
    val c = oldClass.getDeclaredClasses.filter(c => c.getSimpleName == "PreferenceLayout").head
    fieldResId = c.getDeclaredField("resId")
    fieldResId.setAccessible(true)
    fieldWidgetResId = c.getDeclaredField("widgetResId")
    fieldWidgetResId.setAccessible(true)
    preferenceViewHolderConstructor.setAccessible(true)
  }
}

class PreferenceGroupAdapter(group: PreferenceGroup) extends Old(group) {
  import PreferenceGroupAdapter._

  protected lazy val preferenceLayouts: util.List[AnyRef] =
    preferenceLayoutsField.get(this).asInstanceOf[util.List[AnyRef]]

  override def onCreateViewHolder(parent: ViewGroup, viewType: Int): PreferenceViewHolder =
    if (Build.VERSION.SDK_INT < 21) {
      val context = parent.getContext
      val inflater = LayoutInflater.from(context)
      val pl = preferenceLayouts.get(viewType)
      val view = inflater.inflate(fieldResId.get(pl).asInstanceOf[Int], parent, false)
      if (view.getBackground == null) {
        val array = context.obtainStyledAttributes(null, R.styleable.BackgroundStyle)
        var background = array.getDrawable(R.styleable.BackgroundStyle_android_selectableItemBackground)
        if (background == null)
          background = ContextCompat.getDrawable(context, android.R.drawable.list_selector_background)
        array.recycle()
        val (s, t, e, b) = (ViewCompat.getPaddingStart(view), view.getPaddingTop,
          ViewCompat.getPaddingEnd(view), view.getPaddingBottom)
        view.setBackground(background)
        ViewCompat.setPaddingRelative(view, s, t, e, b)
      }
      val widgetFrame = view.findViewById[ViewGroup](android.R.id.widget_frame)
      if (widgetFrame != null) {
        val widgetResId = fieldWidgetResId.get(pl).asInstanceOf[Int]
        if (widgetResId != 0) inflater.inflate(widgetResId, widgetFrame) else widgetFrame.setVisibility(View.GONE)
      }
      preferenceViewHolderConstructor.newInstance(view)
    } else super.onCreateViewHolder(parent, viewType)
}
