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
import android.content.res.Configuration
import android.os.Build
import android.os.Bundle
import com.google.android.material.snackbar.Snackbar
import com.google.android.material.textfield.TextInputLayout
import androidx.core.content.ContextCompat
import androidx.appcompat.app.AlertDialog
import androidx.recyclerview.widget.DefaultItemAnimator
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import androidx.appcompat.widget.Toolbar
import androidx.recyclerview.widget.ItemTouchHelper
import android.text.Editable
import android.text.TextWatcher
import android.view.*
import android.widget.*
import androidx.core.content.getSystemService
import com.github.shadowsocks.App.Companion.app
import com.github.shadowsocks.MainActivity
import com.github.shadowsocks.R
import com.github.shadowsocks.ToolbarFragment
import com.github.shadowsocks.bg.BaseService
import com.github.shadowsocks.utils.*
import com.github.shadowsocks.widget.UndoSnackbarManager
import java.net.IDN
import java.net.MalformedURLException
import java.net.URL
import java.util.*

class CustomRulesFragment : ToolbarFragment(), Toolbar.OnMenuItemClickListener, ActionMode.Callback {
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
    private inner class AclRuleDialog(item: Any = "") : TextWatcher, AdapterView.OnItemSelectedListener {
        val builder: AlertDialog.Builder
        val templateSelector: Spinner
        val editText: EditText
        private val inputLayout: TextInputLayout
        private lateinit var dialog: AlertDialog
        private lateinit var positive: Button

        init {
            val activity = requireActivity()
            val view = activity.layoutInflater.inflate(R.layout.dialog_acl_rule, null)
            templateSelector = view.findViewById(R.id.template_selector)
            editText = view.findViewById(R.id.content)
            inputLayout = view.findViewById(R.id.content_layout)
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
            templateSelector.onItemSelectedListener = this
            editText.addTextChangedListener(this)
            builder = AlertDialog.Builder(activity)
                    .setTitle(R.string.edit_rule)
                    .setNegativeButton(android.R.string.cancel, null)
                    .setView(view)
        }

        fun show() {
            dialog = builder.create()
            dialog.show()
            positive = dialog.getButton(AlertDialog.BUTTON_POSITIVE)
            validate()
        }

        override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) { }
        override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) { }
        override fun afterTextChanged(s: Editable) = validate(value = s)
        override fun onNothingSelected(parent: AdapterView<*>?) = check(false)
        override fun onItemSelected(parent: AdapterView<*>?, view: View?, position: Int, id: Long) = validate(position)

        private fun validate(template: Int = templateSelector.selectedItemPosition, value: Editable = editText.text) {
            val error = when (Template.values()[template]) {
                Template.Generic -> if (value.isEmpty()) "" else null
                Template.Domain -> try {
                    IDN.toASCII(value.toString(), IDN.ALLOW_UNASSIGNED or IDN.USE_STD3_ASCII_RULES)
                    null
                } catch (e: IllegalArgumentException) {
                    e.cause?.message ?: e.message
                }
                Template.Url -> try {
                    URL(value.toString())
                    null
                } catch (e: MalformedURLException) {
                    e.message
                }
            }
            inputLayout.error = error
            positive.isEnabled = error == null
        }

        fun add(): Int? {
            val text = editText.text.toString()
            return when (Template.values()[templateSelector.selectedItemPosition]) {
                Template.Generic -> adapter.addToProxy(text)
                Template.Domain -> adapter.addHostname(TEMPLATE_REGEX_DOMAIN.format(Locale.ENGLISH, IDN.toASCII(text,
                        IDN.ALLOW_UNASSIGNED or IDN.USE_STD3_ASCII_RULES).replace(".", "\\.")))
                Template.Url -> adapter.addURL(URL(text))
            }
        }
    }

    private inner class AclRuleViewHolder(view: View) : RecyclerView.ViewHolder(view),
            View.OnClickListener, View.OnLongClickListener {
        lateinit var item: Any
        private val text = view.findViewById<TextView>(android.R.id.text1)

        init {
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
                val dialog = AclRuleDialog(item)
                dialog.builder
                        .setNeutralButton(R.string.delete) { _, _ ->
                            adapter.remove(item)
                            undoManager.remove(Pair(-1, item))
                        }
                        .setPositiveButton(android.R.string.ok) { _, _ ->
                            adapter.remove(item)
                            val index = dialog.add() ?: adapter.add(item)
                            if (index != null) list.post { list.scrollToPosition(index) }
                        }
                dialog.show()
            }
        }
        override fun onLongClick(p0: View?): Boolean {
            if (!selectedItems.add(item)) selectedItems.remove(item)    // toggle
            onSelectedItemsUpdated()
            itemView.isSelected = !itemView.isSelected
            return true
        }
    }

    private inner class AclRulesAdapter : RecyclerView.Adapter<AclRuleViewHolder>() {
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
    private var mode: ActionMode? = null
    private lateinit var undoManager: UndoSnackbarManager<Any>
    private val clipboard by lazy { requireContext().getSystemService<ClipboardManager>()!! }

    private fun onSelectedItemsUpdated() {
        if (selectedItems.isEmpty()) mode?.finish() else if (mode == null) mode = toolbar.startActionMode(this)
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
        val activity = requireActivity()
        list = view.findViewById(R.id.list)
        list.layoutManager = LinearLayoutManager(activity, RecyclerView.VERTICAL, false)
        list.itemAnimator = DefaultItemAnimator()
        list.adapter = adapter
        undoManager = UndoSnackbarManager(activity.findViewById(R.id.snackbar), adapter::undo)
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
        val mode = mode
        return if (mode != null) {
            mode.finish()
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
        R.id.action_manual_settings -> {
            val dialog = AclRuleDialog()
            dialog.builder.setPositiveButton(android.R.string.ok) { _, _ -> dialog.add() }
            dialog.show()
            true
        }
        R.id.action_import -> {
            try {
                check(adapter.addToProxy(clipboard.primaryClip!!.getItemAt(0).text.toString()) != null)
            } catch (exc: Exception) {
                Snackbar.make(requireActivity().findViewById(R.id.snackbar), R.string.action_import_err,
                        Snackbar.LENGTH_LONG).show()
                printLog(exc)
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
        mode?.finish()
        super.onDetach()
    }

    override fun onCreateActionMode(mode: ActionMode, menu: Menu): Boolean {
        val activity = requireActivity()
        val window = activity.window
        // In the end material_grey_100 is used for background, see AppCompatDrawableManager (very complicated)
        // for dark mode, it's roughly 850? (#303030)
        window.statusBarColor = ContextCompat.getColor(activity, when {
            resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK == Configuration.UI_MODE_NIGHT_YES ->
                android.R.color.black
            Build.VERSION.SDK_INT >= 23 -> {
                window.decorView.systemUiVisibility = View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR
                R.color.material_grey_300
            }
            else -> R.color.material_grey_600
        })
        activity.menuInflater.inflate(R.menu.custom_rules_selection, menu)
        toolbar.touchscreenBlocksFocus = true
        return true
    }
    override fun onPrepareActionMode(mode: ActionMode, menu: Menu): Boolean = false
    override fun onActionItemClicked(mode: ActionMode, item: MenuItem): Boolean = when (item.itemId) {
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
        else -> false
    }
    override fun onDestroyActionMode(mode: ActionMode) {
        val activity = requireActivity()
        val window = activity.window
        window.statusBarColor = ContextCompat.getColor(activity,
                activity.theme.resolveResourceId(android.R.attr.statusBarColor))
        window.decorView.systemUiVisibility = View.SYSTEM_UI_FLAG_VISIBLE
        toolbar.touchscreenBlocksFocus = false
        selectedItems.clear()
        onSelectedItemsUpdated()
        adapter.notifyDataSetChanged()
        this.mode = null
    }
}
