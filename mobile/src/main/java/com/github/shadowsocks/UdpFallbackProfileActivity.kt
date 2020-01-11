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

package com.github.shadowsocks

import android.content.res.Resources
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.CheckedTextView
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.Toolbar
import androidx.recyclerview.widget.DefaultItemAnimator
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.database.ProfileManager
import com.github.shadowsocks.plugin.PluginConfiguration
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.utils.SingleInstanceActivity
import com.github.shadowsocks.utils.resolveResourceId
import com.github.shadowsocks.widget.ListHolderListener
import com.github.shadowsocks.widget.ListListener

class UdpFallbackProfileActivity : AppCompatActivity() {
    inner class ProfileViewHolder(view: View) : RecyclerView.ViewHolder(view), View.OnClickListener {
        private var item: Profile? = null
        private val text = itemView.findViewById<CheckedTextView>(android.R.id.text1)

        init {
            view.setBackgroundResource(theme.resolveResourceId(android.R.attr.selectableItemBackground))
            itemView.setOnClickListener(this)
        }

        fun bind(item: Profile?) {
            this.item = item
            if (item == null) text.setText(R.string.plugin_disabled) else text.text = item.formattedName
            text.isChecked = udpFallback == item?.id
        }

        override fun onClick(v: View?) {
            DataStore.udpFallback = item?.id
            DataStore.dirty = true
            finish()
        }
    }

    inner class ProfilesAdapter : RecyclerView.Adapter<ProfileViewHolder>() {
        internal val profiles = (ProfileManager.getActiveProfiles()?.toMutableList() ?: mutableListOf())
                .filter { it.id != editingId && PluginConfiguration(it.plugin ?: "").selected.isEmpty() }

        override fun onBindViewHolder(holder: ProfileViewHolder, position: Int) =
                holder.bind(if (position == 0) null else profiles[position - 1])
        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ProfileViewHolder = ProfileViewHolder(
                LayoutInflater.from(parent.context).inflate(Resources.getSystem()
                        .getIdentifier("select_dialog_singlechoice_material", "layout", "android"), parent, false))
        override fun getItemCount(): Int = 1 + profiles.size
    }

    private var editingId = DataStore.editingId
    private var udpFallback = DataStore.udpFallback
    private val profilesAdapter = ProfilesAdapter()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        if (editingId == null) {
            finish()
            return
        }
        SingleInstanceActivity.register(this) ?: return
        setContentView(R.layout.layout_udp_fallback)
        ListHolderListener.setup(this)

        val toolbar = findViewById<Toolbar>(R.id.toolbar)
        toolbar.setTitle(R.string.udp_fallback)
        toolbar.setNavigationIcon(R.drawable.ic_navigation_close)
        toolbar.setNavigationOnClickListener { finish() }

        findViewById<RecyclerView>(R.id.list).apply {
            setOnApplyWindowInsetsListener(ListListener)
            itemAnimator = DefaultItemAnimator()
            adapter = profilesAdapter
            layoutManager = LinearLayoutManager(this@UdpFallbackProfileActivity, RecyclerView.VERTICAL, false).apply {
                if (DataStore.udpFallback != null) {
                    scrollToPosition(profilesAdapter.profiles.indexOfFirst { it.id == DataStore.udpFallback } + 1)
                }
            }
        }
    }
}
