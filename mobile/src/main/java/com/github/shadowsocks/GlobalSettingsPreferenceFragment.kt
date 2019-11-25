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

package com.github.shadowsocks

import android.app.Activity
import android.content.ClipData
import android.content.ClipboardManager
import android.content.DialogInterface.*
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.text.SpannableStringBuilder
import android.text.Spanned
import android.text.style.TextAppearanceSpan
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.EditText
import android.widget.TextView
import androidx.appcompat.app.AlertDialog
import androidx.core.content.getSystemService
import androidx.preference.EditTextPreference
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreference
import androidx.recyclerview.widget.DefaultItemAnimator
import androidx.recyclerview.widget.ItemTouchHelper
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.github.shadowsocks.bg.BaseService
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.database.SSRSub
import com.github.shadowsocks.database.SSRSubManager
import com.github.shadowsocks.net.TcpFastOpen
import com.github.shadowsocks.preference.*
import com.github.shadowsocks.utils.*
import com.github.shadowsocks.work.SSRSubSyncer
import com.github.shadowsocks.widget.MainListListener
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class GlobalSettingsPreferenceFragment : PreferenceFragmentCompat() {
    companion object {
        private const val REQUEST_BROWSE = 1
    }

    inner class SSRSubViewHolder(view: View) : RecyclerView.ViewHolder(view),
            View.OnClickListener, View.OnLongClickListener {
        internal lateinit var item: SSRSub
        private val text2 = itemView.findViewById<TextView>(android.R.id.text2)
        private val clipboard by lazy { requireContext().getSystemService<ClipboardManager>()!! }

        init {
            itemView.setOnClickListener(this)
            itemView.setOnLongClickListener(this)
        }

        private fun updateText(isShowUrl: Boolean = false) {
            val builder = SpannableStringBuilder().append(this.item.displayName + "\n")
            if (isShowUrl) {
                val start = builder.length
                builder.append(this.item.url)
                builder.setSpan(TextAppearanceSpan(requireContext(), android.R.style.TextAppearance_Small),
                        start, builder.length, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
            }
            text2.text = builder
        }

        fun bind(item: SSRSub) {
            this.item = item
            updateText()
        }

        override fun onClick(v: View?) {
            updateText(true)
        }

        override fun onLongClick(v: View?): Boolean {
            clipboard.setPrimaryClip(ClipData.newPlainText(null, this.item.url))
            return true
        }
    }

    inner class SSRSubAdapter : RecyclerView.Adapter<SSRSubViewHolder>() {
        private val ssrSubs = SSRSubManager.getAllSSRSub().toMutableList()
        private val updated = HashSet<Profile>()

        init {
            setHasStableIds(true)
        }

        override fun onViewAttachedToWindow(holder: SSRSubViewHolder) {}
        override fun onViewDetachedFromWindow(holder: SSRSubViewHolder) {}
        override fun onBindViewHolder(holder: SSRSubViewHolder, position: Int) = holder.bind(ssrSubs[position])
        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): SSRSubViewHolder = SSRSubViewHolder(
                LayoutInflater.from(parent.context).inflate(R.layout.layout_ssr_sub_item, parent, false))

        override fun getItemCount(): Int = ssrSubs.size
        override fun getItemId(position: Int): Long = ssrSubs[position].id

        fun add(ssrSub: SSRSub) {
            val pos = itemCount
            ssrSubs += ssrSub
            notifyItemInserted(pos)
        }

        fun remove(pos: Int) {
            ssrSubs.removeAt(pos)
            notifyItemRemoved(pos)
        }

        fun updateAll() {
            ssrSubs.clear()
            ssrSubs.addAll(SSRSubManager.getAllSSRSub())
            notifyDataSetChanged()
        }
    }

    private val ssrsub by lazy { findPreference<Preference>(Key.ssrSub)!! }
    private val hosts by lazy { findPreference<EditTextPreference>(Key.hosts)!! }
    private val acl by lazy { findPreference<EditTextPreference>(Key.aclUrl)!! }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        preferenceManager.preferenceDataStore = DataStore.publicStore
        DataStore.initGlobal()
        addPreferencesFromResource(R.xml.pref_global)
        findPreference<SwitchPreference>(Key.persistAcrossReboot)!!.setOnPreferenceChangeListener { _, value ->
            BootReceiver.enabled = value as Boolean
            true
        }

        val canToggleLocked = findPreference<Preference>(Key.directBootAware)!!
        if (Build.VERSION.SDK_INT >= 24) canToggleLocked.setOnPreferenceChangeListener { _, newValue ->
            if (Core.directBootSupported && newValue as Boolean) DirectBoot.update() else DirectBoot.clean()
            true
        } else canToggleLocked.remove()

        val tfo = findPreference<SwitchPreference>(Key.tfo)!!
        tfo.isChecked = DataStore.tcpFastOpen
        tfo.setOnPreferenceChangeListener { _, value ->
            if (value as Boolean && !TcpFastOpen.sendEnabled) {
                val result = TcpFastOpen.enable()?.trim()
                if (TcpFastOpen.sendEnabled) true else {
                    (activity as MainActivity).snackbar(
                            if (result.isNullOrEmpty()) getText(R.string.tcp_fastopen_failure) else result).show()
                    false
                }
            } else true
        }
        if (!TcpFastOpen.supported) {
            tfo.isEnabled = false
            tfo.summary = getString(R.string.tcp_fastopen_summary_unsupported, System.getProperty("os.version"))
        }

        hosts.setOnBindEditTextListener(EditTextPreferenceModifiers.Monospace)
        hosts.summaryProvider = HostsSummaryProvider
        val serviceMode = findPreference<Preference>(Key.serviceMode)!!
        val portProxy = findPreference<EditTextPreference>(Key.portProxy)!!
        portProxy.setOnBindEditTextListener(EditTextPreferenceModifiers.Port)
        val portLocalDns = findPreference<EditTextPreference>(Key.portLocalDns)!!
        portLocalDns.setOnBindEditTextListener(EditTextPreferenceModifiers.Port)
        val portTransproxy = findPreference<EditTextPreference>(Key.portTransproxy)!!
        portTransproxy.setOnBindEditTextListener(EditTextPreferenceModifiers.Port)
        val onServiceModeChange = Preference.OnPreferenceChangeListener { _, newValue ->
            val (enabledLocalDns, enabledTransproxy) = when (newValue as String?) {
                Key.modeProxy -> Pair(false, false)
                Key.modeVpn -> Pair(true, false)
                Key.modeTransproxy -> Pair(true, true)
                else -> throw IllegalArgumentException("newValue: $newValue")
            }
            hosts.isEnabled = enabledLocalDns
            portLocalDns.isEnabled = enabledLocalDns
            portTransproxy.isEnabled = enabledTransproxy
            true
        }
        val listener: (BaseService.State) -> Unit = {
            val stopped = it == BaseService.State.Stopped
            tfo.isEnabled = stopped
            serviceMode.isEnabled = stopped
            portProxy.isEnabled = stopped
            if (stopped) onServiceModeChange.onPreferenceChange(null, DataStore.serviceMode) else {
                hosts.isEnabled = false
                portLocalDns.isEnabled = false
                portTransproxy.isEnabled = false
            }
        }
        listener((activity as MainActivity).state)
        MainActivity.stateListener = listener
        serviceMode.onPreferenceChangeListener = onServiceModeChange

        ssrsub.onPreferenceClickListener = Preference.OnPreferenceClickListener { ssrSubDialog(); true }
        val count = SSRSubManager.getAllSSRSub().count()
        ssrsub.summary = resources.getQuantityString(R.plurals.ssrsub_manage_summary, count, count)
        findPreference<SwitchPreference>(Key.ssrSubAutoUpdate)!!.setOnPreferenceChangeListener { _, newValue ->
            SSRSubSyncer.run { if (newValue as Boolean) enqueue() else cancel() }
            true
        }
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        listView.setOnApplyWindowInsetsListener(MainListListener)
    }

    override fun onDisplayPreferenceDialog(preference: Preference?) {
        when (preference) {
            hosts -> BrowsableEditTextPreferenceDialogFragment().apply {
                setKey(hosts.key)
                setTargetFragment(this@GlobalSettingsPreferenceFragment, REQUEST_BROWSE)
            }.show(parentFragmentManager, hosts.key)
            acl -> ActionEditTextPreferenceDialogFragment().apply {
                setKey(acl.key)
                setTargetFragment(this@GlobalSettingsPreferenceFragment, 0)
            }.show(parentFragmentManager, acl.key)
            else -> super.onDisplayPreferenceDialog(preference)
        }
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        when (requestCode) {
            REQUEST_BROWSE -> {
                if (resultCode != Activity.RESULT_OK) return
                val activity = activity as MainActivity
                try {
                    // we read and persist all its content here to avoid content URL permission issues
                    hosts.text = activity.contentResolver.openInputStream(data!!.data!!)!!.bufferedReader().readText()
                } catch (e: Exception) {
                    activity.snackbar(e.readableMessage).show()
                }
            }
            else -> super.onActivityResult(requestCode, resultCode, data)
        }
    }

    override fun onDestroy() {
        MainActivity.stateListener = null
        super.onDestroy()
    }

    private fun ssrSubDialog(waitUpdate: String = "") {
        val view = View.inflate(requireContext(), R.layout.layout_ssr_sub, null)
        val ssusubsList = view.findViewById<RecyclerView>(R.id.ssrsubList)
        val layoutManager = LinearLayoutManager(requireContext())
        val ssrsubAdapter = SSRSubAdapter()

        ssusubsList.layoutManager = layoutManager
        ssusubsList.itemAnimator = DefaultItemAnimator()
        ssusubsList.adapter = ssrsubAdapter

        ItemTouchHelper(object : ItemTouchHelper.SimpleCallback(0,
                ItemTouchHelper.START or ItemTouchHelper.END) {
            override fun onSwiped(viewHolder: RecyclerView.ViewHolder, direction: Int) {
                val index = viewHolder.adapterPosition
                AlertDialog.Builder(requireContext())
                        .setTitle(getString(R.string.ssrsub_remove_tip_title))
                        .setPositiveButton(R.string.ssrsub_remove_tip_direct) { _, _ ->
                            ssrsubAdapter.remove(index)
                            SSRSubManager.delSSRSub((viewHolder as SSRSubViewHolder).item.id)
                        }
                        .setNegativeButton(android.R.string.no) { _, _ ->
                            ssrsubAdapter.notifyItemChanged(index)
                        }
                        .setNeutralButton(R.string.ssrsub_remove_tip_delete) { _, _ ->
                            val item = (viewHolder as SSRSubViewHolder).item
                            SSRSubManager.deletProfiles(item)
                            ssrsubAdapter.remove(viewHolder.getAdapterPosition())
                            SSRSubManager.delSSRSub(item.id)
                        }
                        .setMessage(getString(R.string.ssrsub_remove_tip))
                        .setCancelable(false)
                        .create()
                        .show()
            }

            override fun onMove(recyclerView: RecyclerView, viewHolder: RecyclerView.ViewHolder,
                                target: RecyclerView.ViewHolder): Boolean = true
        }).attachToRecyclerView(ssusubsList)

        val dialog = AlertDialog.Builder(requireContext())
                .setTitle(getString(R.string.ssrsub_list))
                .setPositiveButton(android.R.string.ok) { _, _ ->
                    ssrsub.summary = resources.getQuantityString(R.plurals.ssrsub_manage_summary,
                            ssrsubAdapter.itemCount, ssrsubAdapter.itemCount)
                }
                .setNegativeButton(R.string.update, null)
                .setNeutralButton(R.string.ssrsub_add) { _, _ ->
                    val editText = EditText(requireContext())
                    AlertDialog.Builder(requireContext())
                            .setTitle(getString(R.string.ssrsub_add))
                            .setPositiveButton(android.R.string.ok) { _, _ ->
                                ssrSubDialog(editText.text.toString())
                            }
                            .setNegativeButton(android.R.string.no) { _, _ -> ssrSubDialog() }
                            .setView(editText)
                            .create()
                            .show()
                }
                .setCancelable(false)
                .setView(view)
                .create()
                .apply {
                    show()
                    getButton(AlertDialog.BUTTON_NEGATIVE).setOnClickListener {
                        getButton(AlertDialog.BUTTON_POSITIVE).isEnabled = false
                        getButton(AlertDialog.BUTTON_NEGATIVE).isEnabled = false
                        getButton(AlertDialog.BUTTON_NEUTRAL).isEnabled = false
                        GlobalScope.launch(Dispatchers.IO) {
                            SSRSubManager.updateAll()
                            withContext(Dispatchers.Main.immediate) {
                                getButton(AlertDialog.BUTTON_POSITIVE).isEnabled = true
                                getButton(AlertDialog.BUTTON_NEUTRAL).isEnabled = true
                                getButton(AlertDialog.BUTTON_NEGATIVE).text = getString(R.string.update_success)
                                ssrsubAdapter.updateAll()
                            }
                        }
                    }
                }

        if (waitUpdate.isEmpty()) return

        listOf(BUTTON_POSITIVE, BUTTON_NEGATIVE, BUTTON_NEUTRAL)
                .forEach { dialog.getButton(it).isEnabled = false }

        GlobalScope.launch(Dispatchers.IO) {
            val new = SSRSubManager.create(waitUpdate)
            withContext(Dispatchers.Main.immediate) {
                if (new != null) ssrsubAdapter.add(new)
                listOf(BUTTON_POSITIVE, BUTTON_NEGATIVE, BUTTON_NEUTRAL)
                        .forEach { dialog.getButton(it).isEnabled = true }
            }
        }
    }
}
