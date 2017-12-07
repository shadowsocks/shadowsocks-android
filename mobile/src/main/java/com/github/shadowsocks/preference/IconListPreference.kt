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

import android.content.Context
import android.graphics.drawable.Drawable
import android.support.v7.preference.ListPreference
import android.util.AttributeSet

class IconListPreference(context: Context, attrs: AttributeSet? = null) : ListPreference(context, attrs) {
    var entryIcons: Array<Drawable?>? = null
    val selectedEntry: Int get() = entryValues.indexOf(value)
    val entryIcon: Drawable? get() = try {
        entryIcons?.get(selectedEntry)
    } catch (_: ArrayIndexOutOfBoundsException) {
        null
    }
//    fun setEntryIcons(@ArrayRes entryIconsResId: Int) {
//        val array = getContext().getResources().obtainTypedArray(entryIconsResId)
//        entryIcons = Array(array.length(), { i -> array.getDrawable(i) })
//        array.recycle()
//    }

    var unknownValueSummary: String? = null
    var entryPackageNames: Array<String>? = null

    private var listener: OnPreferenceChangeListener? = null
    override fun getOnPreferenceChangeListener(): OnPreferenceChangeListener? = listener
    override fun setOnPreferenceChangeListener(listener: OnPreferenceChangeListener?) {
        this.listener = listener
    }

    init {
        super.setOnPreferenceChangeListener({ preference, newValue ->
            val listener = listener
            if (listener == null || listener.onPreferenceChange(preference, newValue)) {
                value = newValue.toString()
                checkSummary()
                if (entryIcons != null) icon = entryIcon
                true
            } else false
        })
//        val a = context.obtainStyledAttributes(attrs, R.styleable.IconListPreference)
//        val entryIconsResId: Int = a.getResourceId(R.styleable.IconListPreference_entryIcons, -1)
//        if (entryIconsResId != -1) entryIcons = entryIconsResId
//        a.recycle()
    }

    fun checkSummary() {
        val unknownValueSummary = unknownValueSummary
        if (unknownValueSummary != null) summary = if (selectedEntry < 0) unknownValueSummary.format(value) else "%s"
    }

    fun init() {
        icon = entryIcon
    }
    override fun onSetInitialValue(restoreValue: Boolean, defaultValue: Any?) {
        super.onSetInitialValue(restoreValue, defaultValue)
        init()
    }
}
