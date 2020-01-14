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

import android.annotation.SuppressLint
import android.content.ClipData
import android.content.ClipboardManager
import android.content.DialogInterface
import android.content.Intent
import android.content.res.Configuration
import android.os.Build
import android.os.Bundle
import android.os.Parcelable
import android.text.Editable
import android.text.TextWatcher
import android.view.*
import android.widget.*
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.widget.Toolbar
import androidx.core.content.ContextCompat
import androidx.core.content.getSystemService
import androidx.recyclerview.widget.DefaultItemAnimator
import androidx.recyclerview.widget.ItemTouchHelper
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.github.shadowsocks.Core
import com.github.shadowsocks.MainActivity
import com.github.shadowsocks.R
import com.github.shadowsocks.ToolbarFragment
import com.github.shadowsocks.bg.BaseService
import com.github.shadowsocks.net.Subnet
import com.github.shadowsocks.plugin.AlertDialogFragment
import com.github.shadowsocks.utils.asIterable
import com.github.shadowsocks.utils.readableMessage
import com.github.shadowsocks.utils.resolveResourceId
import com.github.shadowsocks.widget.ListHolderListener
import com.github.shadowsocks.widget.MainListListener
import com.github.shadowsocks.widget.UndoSnackbarManager
import com.google.android.material.textfield.TextInputLayout
import kotlinx.android.parcel.Parcelize
import me.zhanghai.android.fastscroll.FastScrollerBuilder
import java.net.IDN
import java.net.MalformedURLException
import java.net.URL
import java.util.*
import java.util.regex.PatternSyntaxException

class CustomRulesFragment : ToolbarFragment(), Toolbar.OnMenuItemClickListener, ActionMode.Callback {
    companion object {
        private const val REQUEST_CODE_ADD = 1
        private const val REQUEST_CODE_EDIT = 2

        private const val SELECTED_SUBNETS = "com.github.shadowsocks.acl.CustomRulesFragment.SELECTED_SUBNETS"
        private const val SELECTED_HOSTNAMES = "com.github.shadowsocks.acl.CustomRulesFragment.SELECTED_HOSTNAMES"
        private const val SELECTED_URLS = "com.github.shadowsocks.acl.CustomRulesFragment.SELECTED_URLS"

        // unescaped lol: (?<=^(?:\(\^\|\\\.\)|\^\(\.\*\\\.\)\?|\(\?:\^\|\\\.\))).*(?=\$$)
        private val domainPattern =
                "(?<=^(?:\\(\\^\\|\\\\\\.\\)|\\^\\(\\.\\*\\\\\\.\\)\\?|\\(\\?:\\^\\|\\\\\\.\\))).*(?=\\\$\$)".toRegex()

        @Suppress("FunctionName")
        private fun AclItem(item: Any) = when (item) {
            is String -> AclItem(item, false)
            is Subnet -> AclItem(item.toString(), false)
            is URL -> AclItem(item.toString(), true)
            else -> throw IllegalArgumentException("item")
        }
    }

    private enum class Template {
        Generic,
        Domain,
        Url;
    }
    @Parcelize
    data class AclItem(val item: String = "", val isUrl: Boolean = false) : Parcelable {
        fun toAny() = if (isUrl) URL(item) else Subnet.fromString(item) ?: item
    }
    @Parcelize
    data class AclEditResult(val edited: AclItem, val replacing: AclItem) : Parcelable
    class AclRuleDialogFragment : AlertDialogFragment<AclItem, AclEditResult>(),
            TextWatcher, AdapterView.OnItemSelectedListener {
        private lateinit var templateSelector: Spinner
        private lateinit var editText: EditText
        private lateinit var inputLayout: TextInputLayout
        private val positive by lazy { (dialog as AlertDialog).getButton(AlertDialog.BUTTON_POSITIVE) }

        override fun AlertDialog.Builder.prepare(listener: DialogInterface.OnClickListener) {
            val activity = requireActivity()
            @SuppressLint("InflateParams")
            val view = activity.layoutInflater.inflate(R.layout.dialog_acl_rule, null)
            templateSelector = view.findViewById(R.id.template_selector)
            editText = view.findViewById(R.id.content)
            inputLayout = view.findViewById(R.id.content_layout)
            templateSelector.setSelection(Template.Generic.ordinal)
            editText.setText(arg.item)
            when {
                arg.isUrl -> templateSelector.setSelection(Template.Url.ordinal)
                Subnet.fromString(arg.item) == null -> {
                    val match = domainPattern.find(arg.item)
                    if (match != null) {
                        templateSelector.setSelection(Template.Domain.ordinal)
                        editText.setText(IDN.toUnicode(match.value.replace("\\.", "."),
                                IDN.ALLOW_UNASSIGNED or IDN.USE_STD3_ASCII_RULES))
                    }
                }
            }
            templateSelector.onItemSelectedListener = this@AclRuleDialogFragment
            editText.addTextChangedListener(this@AclRuleDialogFragment)
            setTitle(R.string.edit_rule)
            setPositiveButton(android.R.string.ok, listener)
            setNegativeButton(android.R.string.cancel, null)
            if (arg.item.isNotEmpty()) setNeutralButton(R.string.delete, listener)
            setView(view)
        }

        override fun onStart() {
            super.onStart()
            validate()
        }

        override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) { }
        override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) { }
        override fun afterTextChanged(s: Editable) = validate(value = s)
        override fun onNothingSelected(parent: AdapterView<*>?) = check(false)
        override fun onItemSelected(parent: AdapterView<*>?, view: View?, position: Int, id: Long) = validate(position)

        private fun validate(template: Int = templateSelector.selectedItemPosition, value: Editable = editText.text) {
            var message = ""
            positive.isEnabled = when (Template.values()[template]) {
                Template.Generic -> value.toString().run {
                    try {
                        if (Subnet.fromString(this) == null) toPattern()
                        true
                    } catch (e: PatternSyntaxException) {
                        message = e.readableMessage
                        false
                    }
                }
                Template.Domain -> try {
                    IDN.toASCII(value.toString(), IDN.ALLOW_UNASSIGNED or IDN.USE_STD3_ASCII_RULES)
                    true
                } catch (e: IllegalArgumentException) {
                    message = e.cause?.readableMessage ?: e.readableMessage
                    false
                }
                Template.Url -> try {
                    val url = URL(value.toString())
                    if ("http".equals(url.protocol, true)) message = getString(R.string.cleartext_http_warning)
                    true
                } catch (e: MalformedURLException) {
                    message = e.readableMessage
                    false
                }
            }
            inputLayout.error = message
        }

        override fun ret(which: Int) = when (which) {
            DialogInterface.BUTTON_POSITIVE -> {
                AclEditResult(editText.text.toString().let { text ->
                    when (Template.values()[templateSelector.selectedItemPosition]) {
                        Template.Generic -> AclItem(text)
                        Template.Domain -> AclItem(IDN.toASCII(text, IDN.ALLOW_UNASSIGNED or IDN.USE_STD3_ASCII_RULES)
                                .replace(".", "\\.").let { "(?:^|\\.)$it\$" })
                        Template.Url -> AclItem(text, true)
                    }
                }, arg)
            }
            DialogInterface.BUTTON_NEUTRAL -> AclEditResult(arg, arg)
            else -> null
        }

        override fun onClick(dialog: DialogInterface?, which: Int) {
            if (which != DialogInterface.BUTTON_NEGATIVE) super.onClick(dialog, which)
        }
    }

    private inner class AclRuleViewHolder(view: View) : RecyclerView.ViewHolder(view),
            View.OnClickListener, View.OnLongClickListener {
        lateinit var item: Any
        private val text = view.findViewById<TextView>(android.R.id.text1)

        init {
            view.isFocusable = true
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
            if (selectedItems.isNotEmpty()) onLongClick(v)
            else AclRuleDialogFragment().withArg(AclItem(item)).show(this@CustomRulesFragment, REQUEST_CODE_EDIT)
        }
        override fun onLongClick(v: View?): Boolean {
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
                val k = j - acl.proxyHostnames.size()
                if (k < 0) holder.bind(acl.proxyHostnames[j]) else holder.bind(acl.urls[k])
            }
        }
        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int) = AclRuleViewHolder(LayoutInflater
                .from(parent.context).inflate(android.R.layout.simple_list_item_1, parent, false))
        override fun getItemCount(): Int = acl.subnets.size() + acl.proxyHostnames.size() + acl.urls.size()

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
            val old = acl.proxyHostnames.size()
            val index = acl.subnets.size() + acl.proxyHostnames.add(hostname)
            if (old != acl.proxyHostnames.size()) {
                notifyItemInserted(index)
                apply()
            }
            return index
        }
        fun addURL(url: URL): Int {
            val old = acl.urls.size()
            val index = acl.subnets.size() + acl.proxyHostnames.size() + acl.urls.add(url)
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
            (acl.proxyHostnames.asIterable().asSequence().map { addHostname(it) } +
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
                val k = j - acl.proxyHostnames.size()
                if (k < 0) {
                    undoManager.remove(Pair(j, acl.proxyHostnames[j]))
                    acl.proxyHostnames.removeItemAt(j)
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
                    notifyItemRemoved(acl.subnets.size() + acl.proxyHostnames.indexOf(item))
                    acl.proxyHostnames.remove(item)
                    apply()
                }
                is URL -> {
                    notifyItemRemoved(acl.subnets.size() + acl.proxyHostnames.size() + acl.urls.indexOf(item))
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
            selectedItems.addAll(acl.proxyHostnames.asIterable())
            selectedItems.addAll(acl.urls.asIterable())
            onSelectedItemsUpdated()
            notifyDataSetChanged()
        }
    }

    private val isEnabled get() = (activity as MainActivity).state == BaseService.State.Stopped ||
            Core.currentProfile?.first?.route != Acl.CUSTOM_RULES

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
        view.setOnApplyWindowInsetsListener(ListHolderListener)
        if (savedInstanceState != null) {
            selectedItems.addAll(savedInstanceState.getStringArray(SELECTED_SUBNETS)
                    ?.mapNotNull { Subnet.fromString(it) } ?: listOf())
            selectedItems.addAll(savedInstanceState.getStringArray(SELECTED_HOSTNAMES)
                    ?: arrayOf())
            selectedItems.addAll(savedInstanceState.getStringArray(SELECTED_URLS)?.map { URL(it) }
                    ?: listOf())
            onSelectedItemsUpdated()
        }
        toolbar.setTitle(R.string.custom_rules)
        toolbar.inflateMenu(R.menu.custom_rules_menu)
        toolbar.setOnMenuItemClickListener(this)
        val activity = activity as MainActivity
        list = view.findViewById(R.id.list)
        list.setOnApplyWindowInsetsListener(MainListListener)
        list.layoutManager = LinearLayoutManager(activity, RecyclerView.VERTICAL, false)
        list.itemAnimator = DefaultItemAnimator()
        list.adapter = adapter
        FastScrollerBuilder(list).useMd2Style().build()
        undoManager = UndoSnackbarManager(activity, adapter::undo)
        ItemTouchHelper(object : ItemTouchHelper.SimpleCallback(0, ItemTouchHelper.START) {
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
                is String -> acl.proxyHostnames.add(it)
                is URL -> acl.urls.add(it)
            }
        }
        clipboard.setPrimaryClip(ClipData.newPlainText(null, acl.toString()))
    }

    override fun onMenuItemClick(item: MenuItem): Boolean = when (item.itemId) {
        R.id.action_manual_settings -> {
            AclRuleDialogFragment().withArg(AclItem()).show(this, REQUEST_CODE_ADD)
            true
        }
        R.id.action_import_clipboard -> {
            try {
                check(adapter.addToProxy(clipboard.primaryClip!!.getItemAt(0).text.toString()) != null)
            } catch (exc: Exception) {
                (activity as MainActivity).snackbar().setText(R.string.action_import_err).show()
                exc.printStackTrace()
            }
            true
        }
        R.id.action_import_gfwlist -> {
            val acl = Acl().fromId(Acl.GFWLIST)
            if (acl.bypass) acl.subnets.asIterable().forEach { adapter.addSubnet(it) }
            acl.proxyHostnames.asIterable().forEach { adapter.addHostname(it) }
            acl.urls.asIterable().forEach { adapter.addURL(it) }
            true
        }
        else -> false
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        val editing = when (requestCode) {
            REQUEST_CODE_ADD -> false
            REQUEST_CODE_EDIT -> true
            else -> return super.onActivityResult(requestCode, resultCode, data)
        }
        val ret by lazy { AlertDialogFragment.getRet<AclEditResult>(data!!) }
        when (resultCode) {
            DialogInterface.BUTTON_POSITIVE -> {
                if (editing) adapter.remove(ret.replacing.toAny())
                adapter.add(ret.edited.toAny())?.also { list.post { list.scrollToPosition(it) } }
            }
            DialogInterface.BUTTON_NEUTRAL -> ret.replacing.toAny().let { item ->
                adapter.remove(item)
                undoManager.remove(Pair(-1, item))
            }
        }
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
