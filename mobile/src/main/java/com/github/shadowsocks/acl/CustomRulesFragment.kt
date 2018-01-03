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

package com.github.shadowsocks.acl

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.os.Bundle
import android.support.design.widget.Snackbar
import android.support.v7.app.AlertDialog
import android.support.v7.widget.DefaultItemAnimator
import android.support.v7.widget.LinearLayoutManager
import android.support.v7.widget.RecyclerView
import android.support.v7.widget.Toolbar
import android.support.v7.widget.helper.ItemTouchHelper
import android.view.LayoutInflater
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import android.widget.EditText
import android.widget.Spinner
import android.widget.TextView
import android.widget.Toast
import com.futuremind.recyclerviewfastscroll.FastScroller
import com.futuremind.recyclerviewfastscroll.SectionTitleProvider
import com.github.shadowsocks.App.Companion.app
import com.github.shadowsocks.MainActivity
import com.github.shadowsocks.R
import com.github.shadowsocks.ToolbarFragment
import com.github.shadowsocks.bg.BaseService
import com.github.shadowsocks.utils.Subnet
import com.github.shadowsocks.utils.asIterable
import com.github.shadowsocks.widget.UndoSnackbarManager
import java.net.IDN
import java.net.URL
import java.util.*

class CustomRulesFragment : ToolbarFragment(), Toolbar.OnMenuItemClickListener {
    companion object {
        private const val TEMPLATE_REGEX_DOMAIN = "(^|\\.)%s$"

        private const val SELECTED_SUBNETS = "com.github.shadowsocks.acl.CustomRulesFragment.SELECTED_SUBNETS"
        private const val SELECTED_HOSTNAMES = "com.github.shadowsocks.acl.CustomRulesFragment.SELECTED_HOSTNAMES"
        private const val SELECTED_URLS = "com.github.shadowsocks.acl.CustomRulesFragment.SELECTED_URLS"

        // unescaped: (?<=^(\(\^\|\\\.\)|\^\(\.\*\\\.\)\?)).*(?=\$$)
        private val PATTERN_DOMAIN = "(?<=^(\\(\\^\\|\\\\\\.\\)|\\^\\(\\.\\*\\\\\\.\\)\\?)).*(?=\\\$\$)".toRegex()
    }

    private enum class Template {
        Generic,
        Domain,
        Url;
    }

    private inner class AclRuleViewHolder(view: View) : RecyclerView.ViewHolder(view),
            View.OnClickListener, View.OnLongClickListener {
        lateinit var item: Any
        private val text = view.findViewById<TextView>(android.R.id.text1)

        init {
            view.setPaddingRelative(view.paddingStart, view.paddingTop,
                    Math.max(view.paddingEnd, resources.getDimensionPixelSize(R.dimen.fastscroll__bubble_corner)),
                    view.paddingBottom)
            view.setOnClickListener(this)
            view.setOnLongClickListener(this)
            view.setBackgroundResource(R.drawable.background_selectable)
        }

        fun bind(hostname: String) {
            item = hostname
            text.text = hostname
            itemView.isSelected = selectedItems.contains(hostname)
        }
        fun bind(subnet: Subnet) {
            item = subnet
            text.text = subnet.toString()
            itemView.isSelected = selectedItems.contains(subnet)
        }
        fun bind(url: URL) {
            item = url
            text.text = url.toString()
            itemView.isSelected = selectedItems.contains(url)
        }

        override fun onClick(v: View?) {
            if (selectedItems.isNotEmpty()) onLongClick(v) else {
                val (templateSelector, editText, dialog) = createAclRuleDialog(item)
                dialog
                        .setNeutralButton(R.string.delete, { _, _ ->
                            adapter.remove(item)
                            undoManager.remove(Pair(-1, item))
                        })
                        .setPositiveButton(android.R.string.ok, { _, _ ->
                            adapter.remove(item)
                            val index = adapter.addFromTemplate(templateSelector.selectedItemPosition,
                                    editText.text.toString()) ?: adapter.add(item)
                            if (index != null) list.post { list.scrollToPosition(index) }
                        })
                        .create().show()
            }
        }
        override fun onLongClick(p0: View?): Boolean {
            if (!selectedItems.add(item)) selectedItems.remove(item)    // toggle
            onSelectedItemsUpdated()
            itemView.isSelected = !itemView.isSelected
            return true
        }
    }

    private inner class AclRulesAdapter : RecyclerView.Adapter<AclRuleViewHolder>(), SectionTitleProvider {
        private val acl = Acl.customRules
        private var savePending = false

        override fun onBindViewHolder(holder: AclRuleViewHolder, i: Int) {
            val j = i - acl.subnets.size()
            if (j < 0) holder.bind(acl.subnets[i]) else {
                val k = j - acl.hostnames.size()
                if (k < 0) holder.bind(acl.hostnames[j]) else holder.bind(acl.urls[k])
            }
        }
        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int) = AclRuleViewHolder(LayoutInflater
                .from(parent.context).inflate(android.R.layout.simple_list_item_1, parent, false))
        override fun getItemCount(): Int = acl.subnets.size() + acl.hostnames.size() + acl.urls.size()
        override fun getSectionTitle(i: Int): String {
            val j = i - acl.subnets.size()
            return try {
                (if (j < 0) acl.subnets[i].address.hostAddress.substring(0, 1) else {
                    val k = j - acl.hostnames.size()
                    if (k < 0) {
                        val hostname = acl.hostnames[j]
                        // don't convert IDN yet
                        PATTERN_DOMAIN.find(hostname)?.value?.replace("\\.", ".") ?: hostname
                    } else acl.urls[k].host
                }).substring(0, 1)
            } catch (_: IndexOutOfBoundsException) { " " }
        }

        private fun apply() {
            if (!savePending) {
                savePending = true
                list.post {
                    Acl.customRules = acl
                    savePending = false
                }
            }
        }

        fun add(item: Any): Int? = when (item) {
            is Subnet -> addSubnet(item)
            is String -> addHostname(item)
            is URL -> addURL(item)
            else -> null
        }
        fun addSubnet(subnet: Subnet): Int {
            val old = acl.subnets.size()
            val index = acl.subnets.add(subnet)
            if (old != acl.subnets.size()) {
                notifyItemInserted(index)
                apply()
            }
            return index
        }
        fun addHostname(hostname: String): Int {
            val old = acl.hostnames.size()
            val index = acl.subnets.size() + acl.hostnames.add(hostname)
            if (old != acl.hostnames.size()) {
                notifyItemInserted(index)
                apply()
            }
            return index
        }
        fun addURL(url: URL): Int {
            val old = acl.urls.size()
            val index = acl.subnets.size() + acl.hostnames.size() + acl.urls.add(url)
            if (old != acl.urls.size()) {
                notifyItemInserted(index)
                apply()
            }
            return index
        }
        fun addToProxy(input: String): Int? {
            val acl = Acl().fromReader(input.reader(), true)
            var result: Int? = null
            if (acl.bypass) acl.subnets.asIterable().asSequence().map { addSubnet(it) }
                    .forEach { if (result == null) result = it }
            (acl.hostnames.asIterable().asSequence().map { addHostname(it) } +
                    acl.urls.asIterable().asSequence().map { addURL(it) })
                    .forEach { if (result == null) result = it }
            return result
        }
        fun addFromTemplate(templateValue: Int, text: String): Int? {
            val template = Template.values()[templateValue]
            return when (template) {
                Template.Generic -> addToProxy(text)
                Template.Domain -> try {
                    addHostname(TEMPLATE_REGEX_DOMAIN.format(Locale.ENGLISH, IDN.toASCII(text,
                            IDN.ALLOW_UNASSIGNED or IDN.USE_STD3_ASCII_RULES).replace(".", "\\.")))
                } catch (exc: IllegalArgumentException) {
                    Toast.makeText(activity, exc.message, Toast.LENGTH_SHORT).show()
                    null
                }
                Template.Url -> addURL(URL(text))
            }
        }

        fun remove(i: Int) {
            val j = i - acl.subnets.size()
            if (j < 0) {
                undoManager.remove(Pair(i, acl.subnets[i]))
                acl.subnets.removeItemAt(i)
            } else {
                val k = j - acl.hostnames.size()
                if (k < 0) {
                    undoManager.remove(Pair(j, acl.hostnames[j]))
                    acl.hostnames.removeItemAt(j)
                } else {
                    undoManager.remove(Pair(k, acl.urls[k]))
                    acl.urls.removeItemAt(k)
                }
            }
            notifyItemRemoved(i)
            apply()
        }
        fun remove(item: Any) {
            when (item) {
                is Subnet -> {
                    notifyItemRemoved(acl.subnets.indexOf(item))
                    acl.subnets.remove(item)
                    apply()
                }
                is String -> {
                    notifyItemRemoved(acl.subnets.size() + acl.hostnames.indexOf(item))
                    acl.hostnames.remove(item)
                    apply()
                }
                is URL -> {
                    notifyItemRemoved(acl.subnets.size() + acl.hostnames.size() + acl.urls.indexOf(item))
                    acl.urls.remove(item)
                    apply()
                }
            }
        }
        fun removeSelected() {
            undoManager.remove(selectedItems.map { Pair(0, it) })
            selectedItems.forEach { remove(it) }
            selectedItems.clear()
            onSelectedItemsUpdated()
        }
        fun undo(actions: List<Pair<Int, Any>>) {
            for ((_, item) in actions) add(item)
        }

        fun selectAll() {
            selectedItems.clear()
            selectedItems.addAll(acl.subnets.asIterable())
            selectedItems.addAll(acl.hostnames.asIterable())
            selectedItems.addAll(acl.urls.asIterable())
            onSelectedItemsUpdated()
            notifyDataSetChanged()
        }
    }

    private val isEnabled get() = when ((activity as MainActivity).state) {
        BaseService.CONNECTED -> app.currentProfile?.route != Acl.CUSTOM_RULES
        BaseService.STOPPED -> true
        else -> false
    }

    private val selectedItems = HashSet<Any>()
    private val adapter by lazy { AclRulesAdapter() }
    private lateinit var list: RecyclerView
    private var selectionItem: MenuItem? = null
    private lateinit var undoManager: UndoSnackbarManager<Any>
    private val clipboard by lazy { activity!!.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager }

    private fun onSelectedItemsUpdated() {
        val selectionItem = selectionItem
        if (selectionItem != null) selectionItem.isVisible = selectedItems.isNotEmpty()
    }

    private fun createAclRuleDialog(item: Any = ""): Triple<Spinner, EditText, AlertDialog.Builder> {
        val view = activity!!.layoutInflater.inflate(R.layout.dialog_acl_rule, null)
        val templateSelector = view.findViewById<Spinner>(R.id.template_selector)
        val editText = view.findViewById<EditText>(R.id.content)
        when (item) {
            is String -> {
                val match = PATTERN_DOMAIN.find(item)
                if (match != null) {
                    templateSelector.setSelection(Template.Domain.ordinal)
                    editText.setText(IDN.toUnicode(match.value.replace("\\.", "."),
                            IDN.ALLOW_UNASSIGNED or IDN.USE_STD3_ASCII_RULES))
                } else {
                    templateSelector.setSelection(Template.Generic.ordinal)
                    editText.setText(item)
                }
            }
            is URL -> {
                templateSelector.setSelection(Template.Url.ordinal)
                editText.setText(item.toString())
            }
            else -> {
                templateSelector.setSelection(Template.Generic.ordinal)
                editText.setText(item.toString())
            }
        }
        return Triple(templateSelector, editText, AlertDialog.Builder(activity!!)
                .setTitle(R.string.edit_rule)
                .setNegativeButton(android.R.string.cancel, null)
                .setView(view))
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? =
            inflater.inflate(R.layout.layout_custom_rules, container, false)

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        if (savedInstanceState != null) {
            selectedItems.addAll(savedInstanceState.getStringArray(SELECTED_SUBNETS)
                    ?.mapNotNull(Subnet.Companion::fromString) ?: listOf())
            selectedItems.addAll(savedInstanceState.getStringArray(SELECTED_HOSTNAMES)
                    ?: arrayOf())
            selectedItems.addAll(savedInstanceState.getStringArray(SELECTED_URLS)?.map { URL(it) }
                    ?: listOf())
            onSelectedItemsUpdated()
        }
        toolbar.setTitle(R.string.custom_rules)
        toolbar.inflateMenu(R.menu.custom_rules_menu)
        toolbar.setOnMenuItemClickListener(this)
        val selectionItem = toolbar.menu.findItem(R.id.selection)
        selectionItem.isVisible = selectedItems.isNotEmpty()
        this.selectionItem = selectionItem
        list = view.findViewById(R.id.list)
        list.layoutManager = LinearLayoutManager(activity, LinearLayoutManager.VERTICAL, false)
        list.itemAnimator = DefaultItemAnimator()
        list.adapter = adapter
        view.findViewById<FastScroller>(R.id.fastscroller).setRecyclerView(list)
        undoManager = UndoSnackbarManager(activity!!.findViewById(R.id.snackbar), adapter::undo)
        ItemTouchHelper(object : ItemTouchHelper.SimpleCallback(0, ItemTouchHelper.START or ItemTouchHelper.END) {
            override fun getSwipeDirs(recyclerView: RecyclerView, viewHolder: RecyclerView.ViewHolder): Int =
                    if (isEnabled && selectedItems.isEmpty()) super.getSwipeDirs(recyclerView, viewHolder) else 0
            override fun onSwiped(viewHolder: RecyclerView.ViewHolder, direction: Int) =
                    adapter.remove(viewHolder.adapterPosition)
            override fun onMove(recyclerView: RecyclerView, viewHolder: RecyclerView.ViewHolder,
                                target: RecyclerView.ViewHolder): Boolean = false
        }).attachToRecyclerView(list)
    }

    override fun onBackPressed(): Boolean {
        return if (selectedItems.isNotEmpty()) {
            selectedItems.clear()
            onSelectedItemsUpdated()
            adapter.notifyDataSetChanged()
            true
        } else super.onBackPressed()
    }

    override fun onSaveInstanceState(outState: Bundle) {
        super.onSaveInstanceState(outState)
        outState.putStringArray(SELECTED_SUBNETS, selectedItems.filterIsInstance<Subnet>().map(Subnet::toString)
                .toTypedArray())
        outState.putStringArray(SELECTED_HOSTNAMES, selectedItems.filterIsInstance<String>().toTypedArray())
        outState.putStringArray(SELECTED_URLS, selectedItems.filterIsInstance<URL>().map(URL::toString).toTypedArray())
    }

    private fun copySelected() {
        val acl = Acl()
        acl.bypass = true
        selectedItems.forEach {
            when (it) {
                is Subnet -> acl.subnets.add(it)
                is String -> acl.hostnames.add(it)
                is URL -> acl.urls.add(it)
            }
        }
        clipboard.primaryClip = ClipData.newPlainText(null, acl.toString())
    }

    override fun onMenuItemClick(item: MenuItem): Boolean = when (item.itemId) {
        R.id.action_select_all -> {
            adapter.selectAll()
            true
        }
        R.id.action_cut -> {
            copySelected()
            adapter.removeSelected()
            true
        }
        R.id.action_copy -> {
            copySelected()
            true
        }
        R.id.action_delete -> {
            adapter.removeSelected()
            true
        }

        R.id.action_manual_settings -> {
            val (templateSelector, editText, dialog) = createAclRuleDialog()
            dialog.setPositiveButton(android.R.string.ok, { _, _ ->
                adapter.addFromTemplate(templateSelector.selectedItemPosition, editText.text.toString())
            }).create().show()
            true
        }
        R.id.action_import -> {
            try {
                adapter.addToProxy(clipboard.primaryClip.getItemAt(0).text.toString()) != null
            } catch (exc: Exception) {
                Snackbar.make(activity!!.findViewById(R.id.snackbar), R.string.action_import_err, Snackbar.LENGTH_LONG)
                        .show()
                app.track(exc)
            }
            true
        }
        R.id.action_import_gfwlist -> {
            val acl = Acl().fromId(Acl.GFWLIST)
            if (!acl.bypass) acl.subnets.asIterable().forEach { adapter.addSubnet(it) }
            acl.hostnames.asIterable().forEach { adapter.addHostname(it) }
            acl.urls.asIterable().forEach { adapter.addURL(it) }
            true
        }
        else -> false
    }

    override fun onDetach() {
        undoManager.flush()
        super.onDetach()
    }
}
