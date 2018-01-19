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

import android.app.Dialog
import android.content.ActivityNotFoundException
import android.content.Intent
import android.graphics.Typeface
import android.net.Uri
import android.os.Bundle
import android.provider.Settings
import android.support.design.widget.BottomSheetDialog
import android.support.v7.preference.PreferenceDialogFragmentCompat
import android.support.v7.widget.DefaultItemAnimator
import android.support.v7.widget.LinearLayoutManager
import android.support.v7.widget.RecyclerView
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.TextView
import com.github.shadowsocks.R

class BottomSheetPreferenceDialogFragment : PreferenceDialogFragmentCompat() {
    private inner class IconListViewHolder(val dialog: BottomSheetDialog, view: View) : RecyclerView.ViewHolder(view),
            View.OnClickListener, View.OnLongClickListener {
        private var index = 0
        private val text1 = view.findViewById<TextView>(android.R.id.text1)
        private val text2 = view.findViewById<TextView>(android.R.id.text2)
        private val icon = view.findViewById<ImageView>(android.R.id.icon)

        init {
            view.setOnClickListener(this)
            view.setOnLongClickListener(this)
        }

        fun bind(i: Int, selected: Boolean = false) {
            text1.text = preference.entries[i]
            text2.text = preference.entryValues[i]
            val typeface = if (selected) Typeface.BOLD else Typeface.NORMAL
            text1.setTypeface(null, typeface)
            text2.setTypeface(null, typeface)
            text2.visibility = if (preference.entryValues[i].isNotEmpty() &&
                    preference.entries[i] != preference.entryValues[i]) View.VISIBLE else View.GONE
            icon.setImageDrawable(preference.entryIcons?.get(i))
            index = i
        }

        override fun onClick(p0: View?) {
            clickedIndex = index
            dialog.dismiss()
        }

        override fun onLongClick(p0: View?): Boolean {
            val pn = preference.entryPackageNames?.get(index) ?: return false
            return try {
                startActivity(Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS, Uri.Builder()
                        .scheme("package")
                        .opaquePart(pn)
                        .build()))
                true
            } catch (_: ActivityNotFoundException) {
                false
            }
        }

    }
    private inner class IconListAdapter(private val dialog: BottomSheetDialog) :
            RecyclerView.Adapter<IconListViewHolder>() {
        override fun getItemCount(): Int = preference.entries.size
        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int) = IconListViewHolder(dialog,
                LayoutInflater.from(parent.context).inflate(R.layout.icon_list_item_2, parent, false))
        override fun onBindViewHolder(holder: IconListViewHolder, position: Int) {
            if (preference.selectedEntry < 0) holder.bind(position) else when (position) {
                0 -> holder.bind(preference.selectedEntry, true)
                in preference.selectedEntry + 1 .. Int.MAX_VALUE -> holder.bind(position)
                else -> holder.bind(position - 1)
            }
        }
    }

    private val preference by lazy { getPreference() as IconListPreference }
    private var clickedIndex = -1

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val activity = activity
        val dialog = BottomSheetDialog(activity!!, theme)
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
        if (clickedIndex >= 0 && clickedIndex != preference.selectedEntry) {
            val value = preference.entryValues[clickedIndex].toString()
            if (preference.callChangeListener(value)) preference.value = value
        }
    }
}
