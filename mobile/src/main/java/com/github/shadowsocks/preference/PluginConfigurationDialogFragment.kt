/*******************************************************************************
 *                                                                             *
 *  Copyright (C) 2017 by Max Lv <max.c.lv@gmail.com>                          *
 *  Copyright (C) 2017 by Mygod Studio <contact-shadowsocks-android@mygod.be>  *
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

import android.view.View
import android.widget.EditText
import androidx.appcompat.app.AlertDialog
import androidx.core.os.bundleOf
import androidx.preference.EditTextPreferenceDialogFragmentCompat
import androidx.preference.PreferenceDialogFragmentCompat
import com.github.shadowsocks.ProfileConfigActivity
import com.github.shadowsocks.plugin.PluginContract
import com.github.shadowsocks.plugin.PluginManager

class PluginConfigurationDialogFragment : EditTextPreferenceDialogFragmentCompat() {
    companion object {
        private const val PLUGIN_ID_FRAGMENT_TAG =
                "com.github.shadowsocks.preference.PluginConfigurationDialogFragment.PLUGIN_ID"
    }

    fun setArg(key: String, plugin: String) {
        arguments = bundleOf(PreferenceDialogFragmentCompat.ARG_KEY to key, PLUGIN_ID_FRAGMENT_TAG to plugin)
    }

    private lateinit var editText: EditText

    override fun onPrepareDialogBuilder(builder: AlertDialog.Builder) {
        super.onPrepareDialogBuilder(builder)
        val intent = PluginManager.buildIntent(arguments?.getString(PLUGIN_ID_FRAGMENT_TAG)!!,
                PluginContract.ACTION_HELP)
        val activity = requireActivity()
        if (intent.resolveActivity(activity.packageManager) != null) builder.setNeutralButton("?") { _, _ ->
            activity.startActivityForResult(intent.putExtra(PluginContract.EXTRA_OPTIONS, editText.text.toString()),
                    ProfileConfigActivity.REQUEST_CODE_PLUGIN_HELP)
        }
    }

    override fun onBindDialogView(view: View) {
        super.onBindDialogView(view)
        editText = view.findViewById(android.R.id.edit)
    }
}
