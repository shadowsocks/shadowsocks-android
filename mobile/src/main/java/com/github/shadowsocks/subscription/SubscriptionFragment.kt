/*******************************************************************************
 *                                                                             *
 *  Copyright (C) 2020 by Max Lv <max.c.lv@gmail.com>                          *
 *  Copyright (C) 2020 by Mygod Studio <contact-shadowsocks-android@mygod.be>  *
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

package com.github.shadowsocks.subscription

import android.annotation.SuppressLint
import android.content.DialogInterface
import android.content.Intent
import android.os.Bundle
import android.os.Parcelable
import android.text.Editable
import android.text.TextWatcher
import android.view.*
import android.widget.AdapterView
import android.widget.EditText
import android.widget.TextView
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.widget.Toolbar
import androidx.lifecycle.observe
import androidx.recyclerview.widget.DefaultItemAnimator
import androidx.recyclerview.widget.ItemTouchHelper
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.github.shadowsocks.MainActivity
import com.github.shadowsocks.R
import com.github.shadowsocks.ToolbarFragment
import com.github.shadowsocks.database.SSRSub
import com.github.shadowsocks.database.SSRSubManager
import com.github.shadowsocks.plugin.AlertDialogFragment
import com.github.shadowsocks.utils.printLog
import com.github.shadowsocks.utils.readableMessage
import com.github.shadowsocks.widget.ListHolderListener
import com.github.shadowsocks.widget.MainListListener
import com.google.android.material.textfield.TextInputLayout
import kotlinx.android.parcel.Parcelize
import kotlinx.coroutines.*
import me.zhanghai.android.fastscroll.FastScrollerBuilder
import java.net.MalformedURLException
import java.net.URL

class SubscriptionFragment : ToolbarFragment(), Toolbar.OnMenuItemClickListener {
    companion object {
        private const val REQUEST_CODE_ADD = 1
        private const val REQUEST_CODE_EDIT = 2
    }

    @Parcelize
    data class SubItem(val id: Int = -1, val url: String = "", val url_group: String = "") : Parcelable

    @Parcelize
    data class SubEditResult(val id: Int, val url: String = "") : Parcelable

    class SubDialogFragment : AlertDialogFragment<SubItem, SubEditResult>(),
            TextWatcher, AdapterView.OnItemSelectedListener {
        private lateinit var editText: EditText
        private lateinit var inputLayout: TextInputLayout
        private val positive by lazy { (dialog as AlertDialog).getButton(AlertDialog.BUTTON_POSITIVE) }

        override fun AlertDialog.Builder.prepare(listener: DialogInterface.OnClickListener) {
            val activity = requireActivity()
            @SuppressLint("InflateParams")
            val view = activity.layoutInflater.inflate(R.layout.dialog_subscription, null)
            editText = view.findViewById(R.id.content)
            inputLayout = view.findViewById(R.id.content_layout)
            editText.setText(arg.url)
            if (arg.url_group.isEmpty()) setTitle(R.string.add_subscription) else setTitle(arg.url_group)
            setPositiveButton(android.R.string.ok, listener)
            setNegativeButton(android.R.string.cancel, null)
            if (arg.url.isNotEmpty()) {
                editText.keyListener = null
                editText.setTextIsSelectable(true)
                setNeutralButton(R.string.ssrsub_remove_tip_delete, listener)
            } else {
                editText.addTextChangedListener(this@SubDialogFragment)
            }
            setView(view)
        }

        override fun onStart() {
            super.onStart()
            validate()
        }

        override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
        override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {}
        override fun afterTextChanged(s: Editable) = validate(value = s)
        override fun onNothingSelected(parent: AdapterView<*>?) = check(false)
        override fun onItemSelected(parent: AdapterView<*>?, view: View?, position: Int, id: Long) = validate()

        private fun validate(value: Editable = editText.text) {
            var message = ""
            positive.isEnabled = try {
                val url = URL(value.toString())
                if ("http".equals(url.protocol, true)) {
                    message = getString(R.string.cleartext_http_warning)
                    false
                } else true
            } catch (e: MalformedURLException) {
                message = e.readableMessage
                false
            }
            inputLayout.error = message
        }

        override fun ret(which: Int) = when (which) {
            DialogInterface.BUTTON_POSITIVE -> {
                SubEditResult(arg.id, editText.text.toString())
            }
            DialogInterface.BUTTON_NEUTRAL -> SubEditResult(arg.id)
            else -> null
        }

        override fun onClick(dialog: DialogInterface?, which: Int) {
            if (which != DialogInterface.BUTTON_NEGATIVE) super.onClick(dialog, which)
        }
    }

    private inner class SubViewHolder(view: View) : RecyclerView.ViewHolder(view),
            View.OnClickListener {
        lateinit var item: SSRSub
        private val text = view.findViewById<TextView>(android.R.id.text1)

        init {
            view.isFocusable = true
            view.setOnClickListener(this)
            view.setBackgroundResource(R.drawable.background_selectable)
        }

        @SuppressLint("SetTextI18n")
        fun bind(item: SSRSub) {
            this.item = item
            text.text = "${item.getStatue(requireContext())}\t${item.url_group}"
        }

        override fun onClick(v: View?) {
            if (item.id == 0L) return
            SubDialogFragment().withArg(SubItem(adapterPosition, item.url, item.url_group))
                    .show(this@SubscriptionFragment, REQUEST_CODE_EDIT)
        }
    }

    private inner class SubscriptionAdapter : RecyclerView.Adapter<SubViewHolder>() {
        private val subscription = SSRSubManager.getAllSSRSub().toMutableList()

        override fun onBindViewHolder(holder: SubViewHolder, i: Int) {
            holder.bind(subscription[i])
        }

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int) = SubViewHolder(LayoutInflater
                .from(parent.context).inflate(android.R.layout.simple_list_item_1, parent, false))

        override fun getItemCount(): Int = subscription.size

        fun add(ssrSub: SSRSub): Int {
            subscription.add(ssrSub)
            notifyItemInserted(itemCount)
            return itemCount - 1
        }

        fun remove(i: Int) {
            subscription.removeAt(i)
            notifyItemRemoved(i)
        }

        fun del(i: Int, withProfiles: Boolean = false) {
            if (withProfiles) SSRSubManager.deletProfiles(subscription[i])
            SSRSubManager.delSSRSub(subscription[i].id)
            remove(i)
        }

        fun updateAll() {
            subscription.clear()
            subscription.addAll(SSRSubManager.getAllSSRSub())
            notifyDataSetChanged()
        }

    }

    private val adapter by lazy { SubscriptionAdapter() }
    private lateinit var list: RecyclerView
    private var mode: ActionMode? = null

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? =
            inflater.inflate(R.layout.layout_subscriptions, container, false)

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        view.setOnApplyWindowInsetsListener(ListHolderListener)
        toolbar.setTitle(R.string.subscriptions)
        toolbar.inflateMenu(R.menu.subscription_menu)
        toolbar.setOnMenuItemClickListener(this)
        SubscriptionService.idle.observe(this) {
            toolbar.menu.findItem(R.id.action_update_subscription).isEnabled = it
            if (it == true) adapter.updateAll()
        }
        val activity = activity as MainActivity
        list = view.findViewById(R.id.list)
        list.setOnApplyWindowInsetsListener(MainListListener)
        list.layoutManager = LinearLayoutManager(activity, RecyclerView.VERTICAL, false)
        list.itemAnimator = DefaultItemAnimator()
        list.adapter = adapter
        FastScrollerBuilder(list).useMd2Style().build()
        ItemTouchHelper(object : ItemTouchHelper.SimpleCallback(0, ItemTouchHelper.START) {
            override fun onSwiped(viewHolder: RecyclerView.ViewHolder, direction: Int) {
                val index = viewHolder.adapterPosition
                if ((viewHolder as SubViewHolder).item.id == 0L) adapter.notifyItemChanged(index)
                else adapter.del(index)
            }

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

    override fun onMenuItemClick(item: MenuItem): Boolean = when (item.itemId) {
        R.id.action_manual_settings -> {
            SubDialogFragment().withArg(SubItem()).show(this, REQUEST_CODE_ADD)
            true
        }
        R.id.action_update_subscription -> {
            val context = requireContext()
            context.startService(Intent(context, SubscriptionService::class.java))
            true
        }
        else -> false
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        val ret by lazy { AlertDialogFragment.getRet<SubEditResult>(data!!) }
        when (resultCode) {
            DialogInterface.BUTTON_POSITIVE -> {
                if (requestCode != REQUEST_CODE_ADD) return super.onActivityResult(requestCode, resultCode, data)
                val pos = adapter.add(SSRSub(url = ret.url, url_group = getString(R.string.service_subscription_finishing)))
                list.post { list.scrollToPosition(pos) }
                GlobalScope.launch(Dispatchers.IO) {
                    var new: SSRSub? = null
                    try {
                        new = withTimeout(10000L) { return@withTimeout SSRSubManager.create(ret.url) }
                    } catch (e: Exception) {
                        printLog(e)
                        GlobalScope.launch(Dispatchers.Main) {
                            (activity as MainActivity).snackbar().setText(e.readableMessage).show()
                        }
                    } finally {
                        withContext(Dispatchers.Main) {
                            adapter.remove(pos)
                            if (new != null) adapter.add(new)
                        }
                    }
                }
            }
            DialogInterface.BUTTON_NEUTRAL -> {
                adapter.del(ret.id, true)
            }
        }
    }

    override fun onDetach() {
        mode?.finish()
        super.onDetach()
    }
}
