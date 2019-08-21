/*******************************************************************************
 *                                                                             *
 *  Copyright (C) 2019 by Max Lv <max.c.lv@gmail.com>                          *
 *  Copyright (C) 2019 by Mygod Studio <contact-shadowsocks-android@mygod.be>  *
 *                                                                             *
 *  This program is free software: you can redistribute it and/or modify       *
 *  it under the terms of the GNU General Public License as published by       *
 *  the Free Software Foundation, either version 3 of the License, or          *
 *  (at your option) any later version.                                        *
 *                                                                             *
 *  This program is distributed in the hope that it will be useful,            *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 *  GNU General Public License for more details.                               *
 *                                                                             *
 *  You should have received a copy of the GNU General Public License          *
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.       *
 *                                                                             *
 *******************************************************************************/

package com.github.shadowsocks.widget

import android.view.View
import android.view.WindowInsets
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.updatePadding
import com.github.shadowsocks.R

object ListHolderListener : View.OnApplyWindowInsetsListener {
    override fun onApplyWindowInsets(view: View, insets: WindowInsets): WindowInsets {
        view.updatePadding(insets.systemWindowInsetLeft, insets.systemWindowInsetTop, insets.systemWindowInsetRight)
        @Suppress("DEPRECATION")
        return insets.replaceSystemWindowInsets(0, 0, 0, insets.systemWindowInsetBottom)
    }

    fun setup(activity: AppCompatActivity) = activity.findViewById<View>(android.R.id.content).apply {
        setOnApplyWindowInsetsListener(ListHolderListener)
        systemUiVisibility = View.SYSTEM_UI_FLAG_LAYOUT_STABLE or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
    }
}

object MainListListener : View.OnApplyWindowInsetsListener {
    override fun onApplyWindowInsets(view: View, insets: WindowInsets) = insets.apply {
        view.updatePadding(bottom = view.resources.getDimensionPixelOffset(R.dimen.main_list_padding_bottom) +
                systemWindowInsetBottom)
    }
}

object ListListener : View.OnApplyWindowInsetsListener {
    override fun onApplyWindowInsets(view: View, insets: WindowInsets) = insets.apply {
        view.updatePadding(bottom = systemWindowInsetBottom)
    }
}
