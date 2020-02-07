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

import android.app.Activity
import android.app.Dialog
import android.content.ActivityNotFoundException
import android.content.Intent
import android.graphics.Typeface
import android.net.Uri
import android.os.Bundle
import android.provider.Settings
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.TextView
import androidx.appcompat.widget.TooltipCompat
import androidx.core.os.bundleOf
import androidx.core.view.isGone
import androidx.core.view.isVisible
import androidx.preference.PreferenceDialogFragmentCompat
import androidx.recyclerview.widget.DefaultItemAnimator
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.github.shadowsocks.R
import com.github.shadowsocks.plugin.Plugin
import com.google.android.material.bottomsheet.BottomSheetDialog

class PluginPreferenceDialogFragment : PreferenceDialogFragmentCompat() {
    companion object {
        const val KEY_SELECTED_ID = "id"
    }

    private inner class IconListViewHolder(val dialog: BottomSheetDialog, view: View) : RecyclerView.ViewHolder(view),
            View.OnClickListener, View.OnLongClickListener {
        private lateinit var plugin: Plugin
        private val text1 = view.findViewById<TextView>(android.R.id.text1)
        private val text2 = view.findViewById<TextView>(android.R.id.text2)
        private val icon = view.findViewById<ImageView>(android.R.id.icon)
        private val unlock = view.findViewById<View>(R.id.unlock).apply {
            TooltipCompat.setTooltipText(this, getText(R.string.plugin_auto_connect_unlock_only))
        }

        init {
            view.setOnClickListener(this)
            view.setOnLongClickListener(this)
        }

        fun bind(plugin: Plugin, selected: Boolean = false) {
            this.plugin = plugin
            val label = plugin.label
            text1.text = label
            text2.text = plugin.id
            val typeface = if (selected) Typeface.BOLD else Typeface.NORMAL
            text1.setTypeface(null, typeface)
            text2.setTypeface(null, typeface)
            text2.isVisible = plugin.id.isNotEmpty() && label != plugin.id
            icon.setImageDrawable(plugin.icon)
            unlock.isGone = plugin.directBootAware || !DataStore.persistAcrossReboot
        }

        override fun onClick(v: View?) {
            clicked = plugin
            dialog.dismiss()
        }

        override fun onLongClick(v: View?) = try {
            startActivity(Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS, Uri.Builder()
                    .scheme("package")
                    .opaquePart(plugin.packageName)
                    .build()))
            true
        } catch (_: ActivityNotFoundException) {
            false
        }
    }
    private inner class IconListAdapter(private val dialog: BottomSheetDialog) :
            RecyclerView.Adapter<IconListViewHolder>() {
        override fun getItemCount(): Int = preference.plugins.size
        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int) = IconListViewHolder(dialog,
                LayoutInflater.from(parent.context).inflate(R.layout.icon_list_item_2, parent, false))
        override fun onBindViewHolder(holder: IconListViewHolder, position: Int) {
            if (selected < 0) holder.bind(preference.plugins[position]) else when (position) {
                0 -> holder.bind(preference.selectedEntry!!, true)
                in selected + 1..Int.MAX_VALUE -> holder.bind(preference.plugins[position])
                else -> holder.bind(preference.plugins[position - 1])
            }
        }
    }

    fun setArg(key: String) {
        arguments = bundleOf(ARG_KEY to key)
    }

    private val preference by lazy { getPreference() as PluginPreference }
    private val selected by lazy { preference.plugins.indexOf(preference.selectedEntry) }
    private var clicked: Plugin? = null

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val activity = requireActivity()
        val dialog = BottomSheetDialog(activity, theme)
        val recycler = RecyclerView(activity)
        val padding = resources.getDimensionPixelOffset(R.dimen.bottom_sheet_padding)
        recycler.setPadding(0, padding, 0, padding)
        recycler.setHasFixedSize(true)
        recycler.layoutManager = LinearLayoutManager(activity)
        recycler.itemAnimator = DefaultItemAnimator()
        recycler.adapter = IconListAdapter(dialog)
        recycler.layoutParams = ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
        dialog.setContentView(recycler)
        dialog.findViewById<View>(R.id.touch_outside)!!.isFocusable = false
        return dialog
    }

    override fun onDialogClosed(positiveResult: Boolean) {
        val clicked = clicked
        if (clicked != null && clicked != preference.selectedEntry) {
            targetFragment!!.onActivityResult(targetRequestCode, Activity.RESULT_OK,
                    Intent().putExtra(KEY_SELECTED_ID, clicked.id))
        } else targetFragment!!.onActivityResult(targetRequestCode, Activity.RESULT_CANCELED, null)
    }
}
