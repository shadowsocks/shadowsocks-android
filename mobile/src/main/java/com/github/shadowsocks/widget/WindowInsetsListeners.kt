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
import androidx.appcompat.app.AppCompatActivity
import androidx.core.graphics.Insets
import androidx.core.view.*
import com.github.shadowsocks.R

object ListHolderListener : OnApplyWindowInsetsListener {
    override fun onApplyWindowInsets(view: View, insets: WindowInsetsCompat): WindowInsetsCompat {
        val statusBarInsets = insets.getInsets(WindowInsetsCompat.Type.statusBars())
        view.setPadding(statusBarInsets.left, statusBarInsets.top, statusBarInsets.right, statusBarInsets.bottom)
        return WindowInsetsCompat.Builder(insets).apply {
            setInsets(WindowInsetsCompat.Type.statusBars(), Insets.NONE)
            setInsets(WindowInsetsCompat.Type.navigationBars(),
                    insets.getInsets(WindowInsetsCompat.Type.navigationBars()))
        }.build()
    }

    fun setup(activity: AppCompatActivity) = activity.findViewById<View>(android.R.id.content).let {
        ViewCompat.setOnApplyWindowInsetsListener(it, ListHolderListener)
        WindowCompat.setDecorFitsSystemWindows(activity.window, false)
    }
}

object MainListListener : OnApplyWindowInsetsListener {
    override fun onApplyWindowInsets(view: View, insets: WindowInsetsCompat) = insets.apply {
        view.updatePadding(bottom = view.resources.getDimensionPixelOffset(R.dimen.main_list_padding_bottom) +
                insets.getInsets(WindowInsetsCompat.Type.navigationBars()).bottom)
    }
}

object ListListener : OnApplyWindowInsetsListener {
    override fun onApplyWindowInsets(view: View, insets: WindowInsetsCompat) = insets.apply {
        view.updatePadding(bottom = insets.getInsets(WindowInsetsCompat.Type.navigationBars()).bottom)
    }
}
