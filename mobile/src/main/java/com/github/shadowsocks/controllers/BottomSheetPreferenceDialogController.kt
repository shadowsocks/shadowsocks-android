package com.github.shadowsocks.controllers

import android.app.Dialog
import android.content.ActivityNotFoundException
import android.content.Intent
import android.graphics.Typeface
import android.net.Uri
import android.os.Bundle
import android.provider.Settings
import android.support.design.widget.BottomSheetDialog
import android.support.v7.widget.DefaultItemAnimator
import android.support.v7.widget.LinearLayoutManager
import android.support.v7.widget.RecyclerView
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.TextView
import com.github.shadowsocks.R
import com.github.shadowsocks.preference.IconListPreference
import im.mash.preference.PreferenceDialogController

class BottomSheetPreferenceDialogController : PreferenceDialogController() {
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
            text1.text = iconListPreference.entries[i]
            text2.text = iconListPreference.entryValues[i]
            val typeface = if (selected) Typeface.BOLD else Typeface.NORMAL
            text1.setTypeface(null, typeface)
            text2.setTypeface(null, typeface)
            text2.visibility = if (iconListPreference.entryValues[i].isNotEmpty() &&
                    iconListPreference.entries[i] != iconListPreference.entryValues[i]) View.VISIBLE else View.GONE
            icon.setImageDrawable(iconListPreference.entryIcons?.get(i))
            index = i
        }

        override fun onClick(p0: View?) {
            clickedIndex = index
            dialog.dismiss()
        }

        override fun onLongClick(p0: View?): Boolean {
            val pn = iconListPreference.entryPackageNames?.get(index) ?: return false
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
        override fun getItemCount(): Int = iconListPreference.entries.size
        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int) = IconListViewHolder(dialog,
                LayoutInflater.from(parent.context).inflate(R.layout.icon_list_item_2, parent, false))
        override fun onBindViewHolder(holder: IconListViewHolder, position: Int) {
            if (iconListPreference.selectedEntry < 0) holder.bind(position) else when (position) {
                0 -> holder.bind(iconListPreference.selectedEntry, true)
                in iconListPreference.selectedEntry + 1 .. Int.MAX_VALUE -> holder.bind(position)
                else -> holder.bind(position - 1)
            }
        }
    }

    private val iconListPreference by lazy { this.preference as IconListPreference }
    private var clickedIndex = -1

    override fun onCreateDialog(savedViewState: Bundle?): Dialog {
        val activity = activity!!
        val dialog = BottomSheetDialog(activity)
        val recycler = RecyclerView(activity)
        val padding = resources!!.getDimensionPixelOffset(R.dimen.bottom_sheet_padding)
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
        if (clickedIndex >= 0 && clickedIndex != iconListPreference.selectedEntry) {
            val value = iconListPreference.entryValues[clickedIndex].toString()
            if (iconListPreference.callChangeListener(value)) iconListPreference.value = value
        }
    }
}