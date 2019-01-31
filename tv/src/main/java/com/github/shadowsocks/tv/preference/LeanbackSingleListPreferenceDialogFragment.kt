/*******************************************************************************
 *                                                                             *
 *  Copyright (C) 2018 by Max Lv <max.c.lv@gmail.com>                          *
 *  Copyright (C) 2018 by Mygod Studio <contact-shadowsocks-android@mygod.be>  *
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

package com.github.shadowsocks.tv.preference

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.leanback.preference.LeanbackListPreferenceDialogFragmentCompat
import androidx.recyclerview.widget.RecyclerView

/**
 * Fix: scroll to selected item.
 */
open class LeanbackSingleListPreferenceDialogFragment : LeanbackListPreferenceDialogFragmentCompat() {
    companion object {
        private val mEntryValues = LeanbackListPreferenceDialogFragmentCompat::class.java
                .getDeclaredField("mEntryValues").apply { isAccessible = true }
        private val mInitialSelection = LeanbackListPreferenceDialogFragmentCompat::class.java
                .getDeclaredField("mInitialSelection").apply { isAccessible = true }
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        val selected = mInitialSelection.get(this) as? String
        val index = (mEntryValues.get(this) as? Array<CharSequence?>)?.indexOfFirst { it == selected }
        return super.onCreateView(inflater, container, savedInstanceState)!!.also {
            if (index != null) it.findViewById<RecyclerView>(android.R.id.list).layoutManager!!.scrollToPosition(index)
        }
    }
}
