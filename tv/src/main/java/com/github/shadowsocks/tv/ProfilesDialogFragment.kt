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

package com.github.shadowsocks.tv

import android.os.Bundle
import android.text.format.Formatter
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.leanback.preference.LeanbackListPreferenceDialogFragment
import androidx.recyclerview.widget.RecyclerView
import com.github.shadowsocks.Core
import com.github.shadowsocks.database.ProfileManager
import com.github.shadowsocks.plugin.PluginConfiguration
import com.github.shadowsocks.preference.DataStore

class ProfilesDialogFragment : LeanbackListPreferenceDialogFragment() {
    inner class ProfilesAdapter : RecyclerView.Adapter<ViewHolder>(), ViewHolder.OnItemClickListener {
        private val profiles = ProfileManager.getAllProfiles()!!

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int) = ViewHolder(LayoutInflater
                .from(parent.context).inflate(R.layout.leanback_list_preference_item_single_2, parent, false), this)

        override fun onBindViewHolder(holder: ViewHolder, position: Int) {
            val profile = profiles[position]
            holder.widgetView.isChecked = profile.id == DataStore.profileId
            holder.titleView.text = profile.formattedName
            holder.itemView.findViewById<TextView>(android.R.id.summary).text = ArrayList<String>().apply {
                if (!profile.name.isNullOrEmpty()) this += profile.formattedAddress
                val id = PluginConfiguration(profile.plugin ?: "").selected
                if (id.isNotEmpty()) this += getString(R.string.profile_plugin, id)
                if (profile.tx > 0 || profile.rx > 0) this += getString(R.string.traffic,
                        Formatter.formatFileSize(activity, profile.tx), Formatter.formatFileSize(activity, profile.rx))
            }.joinToString("\n")
        }

        override fun getItemCount() = profiles.size

        override fun onItemClick(viewHolder: ViewHolder) {
            val index = viewHolder.adapterPosition
            if (index == RecyclerView.NO_POSITION) return
            Core.switchProfile(profiles[index].id)
            (targetFragment as MainPreferenceFragment).startService()
            fragmentManager?.popBackStack()
            notifyDataSetChanged()
        }

        val selectedIndex = profiles.indexOfFirst { it.id == DataStore.profileId }
    }

    override fun onCreateAdapter() = ProfilesAdapter()
    override fun onCreateView(inflater: LayoutInflater?, container: ViewGroup?, savedInstanceState: Bundle?): View {
        return super.onCreateView(inflater, container, savedInstanceState)!!.also {
            val list = it.findViewById<RecyclerView>(android.R.id.list)
            list.layoutManager!!.scrollToPosition((list.adapter as ProfilesAdapter).selectedIndex)
        }
    }
}
