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

package com.github.shadowsocks.preference

import android.content.ActivityNotFoundException
import android.content.Intent
import androidx.appcompat.app.AlertDialog
import androidx.core.os.bundleOf
import androidx.preference.EditTextPreferenceDialogFragmentCompat
import com.github.shadowsocks.MainActivity
import com.github.shadowsocks.R

class BrowsableEditTextPreferenceDialogFragment : EditTextPreferenceDialogFragmentCompat() {
    fun setKey(key: String) {
        arguments = bundleOf(Pair(ARG_KEY, key))
    }

    override fun onPrepareDialogBuilder(builder: AlertDialog.Builder) {
        super.onPrepareDialogBuilder(builder)
        builder.setNeutralButton(R.string.browse) { _, _ ->
            val activity = activity as MainActivity
            try {
                targetFragment!!.startActivityForResult(Intent(Intent.ACTION_GET_CONTENT).apply {
                    addCategory(Intent.CATEGORY_OPENABLE)
                    type = "*/*"
                }, targetRequestCode)
                return@setNeutralButton
            } catch (_: ActivityNotFoundException) { } catch (_: SecurityException) { }
            activity.snackbar(activity.getString(R.string.file_manager_missing)).show()
        }
    }
}
