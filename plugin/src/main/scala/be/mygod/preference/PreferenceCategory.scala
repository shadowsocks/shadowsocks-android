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
import android.support.v7.preference.{PreferenceViewHolder, PreferenceCategory => Base}
import android.util.AttributeSet
import android.view.ViewGroup.MarginLayoutParams

/**
  * Based on: https://github.com/Gericop/Android-Support-Preference-V7-Fix/blob/4c2bb2896895dbedb37b659b36bd2a96b33c1605/preference-v7/src/main/java/com/takisoft/fix/support/v7/preference/PreferenceCategory.java
  *
  * @author Mygod
  */
class PreferenceCategory(context: Context, attrs: AttributeSet = null) extends Base(context, attrs) {
  override def onBindViewHolder(holder: PreferenceViewHolder) {
    super.onBindViewHolder(holder)
    holder.findViewById(android.R.id.title).getLayoutParams.asInstanceOf[MarginLayoutParams].bottomMargin = 0
  }
}
