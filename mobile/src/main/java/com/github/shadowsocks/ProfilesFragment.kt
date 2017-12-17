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

import android.annotation.SuppressLint
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.nfc.NdefMessage
import android.nfc.NdefRecord
import android.nfc.NfcAdapter
import android.os.Bundle
import android.support.design.widget.Snackbar
import android.support.v4.app.DialogFragment
import android.support.v7.widget.*
import android.support.v7.widget.helper.ItemTouchHelper
import android.view.*
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast
import com.github.shadowsocks.App.Companion.app
import com.github.shadowsocks.bg.BaseService
import com.github.shadowsocks.bg.TrafficMonitor
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.database.ProfileManager
import com.github.shadowsocks.plugin.PluginConfiguration
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.utils.Action
import com.github.shadowsocks.widget.UndoSnackbarManager
import com.google.android.gms.ads.AdRequest
import com.google.android.gms.ads.AdSize
import com.google.android.gms.ads.AdView
import net.glxn.qrgen.android.QRCode

class ProfilesFragment : ToolbarFragment(), Toolbar.OnMenuItemClickListener {
    companion object {
        /**
         * used for callback from ProfileManager and stateChanged from MainActivity
         */
        var instance: ProfilesFragment? = null

        private const val KEY_URL = "com.github.shadowsocks.QRCodeDialog.KEY_URL"
    }

    /**
     * Is ProfilesFragment editable at all.
     */
    private val isEnabled get() = when ((activity as MainActivity).state) {
        BaseService.CONNECTED, BaseService.STOPPED -> true
        else -> false
    }
    private fun isProfileEditable(id: Int) = when ((activity as MainActivity).state) {
        BaseService.CONNECTED -> id != DataStore.profileId
        BaseService.STOPPED -> true
        else -> false
    }

    @SuppressLint("ValidFragment")
    class QRCodeDialog() : DialogFragment() {

        constructor(url: String) : this() {
            val bundle = Bundle()
            bundle.putString(KEY_URL, url)
            arguments = bundle
        }

        private val url get() = arguments!!.getString(KEY_URL)
        private val nfcShareItem by lazy { url.toByteArray() }
        private var adapter: NfcAdapter? = null

        override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
            val image = ImageView(context)
            image.layoutParams = LinearLayout.LayoutParams(-1, -1)
            val size = resources.getDimensionPixelSize(R.dimen.qr_code_size)
            image.setImageBitmap((QRCode.from(url).withSize(size, size) as QRCode).bitmap())
            return image
        }

        override fun onAttach(context: Context?) {
            super.onAttach(context)
            val adapter = NfcAdapter.getDefaultAdapter(context)
            adapter?.setNdefPushMessage(NdefMessage(arrayOf(
                    NdefRecord(NdefRecord.TNF_ABSOLUTE_URI, nfcShareItem, byteArrayOf(), nfcShareItem))), activity)
            this.adapter = adapter
        }

        override fun onDetach() {
            super.onDetach()
            val activity = activity
            if (activity != null && !activity.isFinishing && !activity.isDestroyed)
                adapter?.setNdefPushMessage(null, activity)
            adapter = null
        }
    }

    inner class ProfileViewHolder(view: View) : RecyclerView.ViewHolder(view),
            View.OnClickListener, PopupMenu.OnMenuItemClickListener {
        internal lateinit var item: Profile

        private val text1 = itemView.findViewById<TextView>(android.R.id.text1)
        private val text2 = itemView.findViewById<TextView>(android.R.id.text2)
        private val traffic = itemView.findViewById<TextView>(R.id.traffic)
        private val edit = itemView.findViewById<View>(R.id.edit)
        private var adView: AdView? = null

        init {
            edit.setOnClickListener { startConfig(item.id) }
            TooltipCompat.setTooltipText(edit, edit.contentDescription)
            itemView.setOnClickListener(this)
            val share = itemView.findViewById<View>(R.id.share)
            share.setOnClickListener {
                val popup = PopupMenu(activity!!, share)
                popup.menuInflater.inflate(R.menu.profile_share_popup, popup.menu)
                popup.setOnMenuItemClickListener(this)
                popup.show()
            }
            TooltipCompat.setTooltipText(share, share.contentDescription)
        }

        fun bind(item: Profile) {
            this.item = item
            val editable = isProfileEditable(item.id)
            edit.isEnabled = editable
            edit.alpha = if (editable) 1F else .5F
            var tx = item.tx
            var rx = item.rx
            if (item.id == bandwidthProfile) {
                tx += txTotal
                rx += rxTotal
            }
            text1.text = item.formattedName
            val t2 = ArrayList<String>()
            if (!item.name.isNullOrEmpty()) t2 += item.formattedAddress
            val id = PluginConfiguration(item.plugin ?: "").selected
            if (id.isNotEmpty()) t2 += app.getString(R.string.profile_plugin, id)
            if (t2.isEmpty()) text2.visibility = View.GONE else {
                text2.visibility = View.VISIBLE
                text2.text = t2.joinToString("\n")
            }
            if (tx <= 0 && rx <= 0) traffic.visibility = View.GONE else {
                traffic.visibility = View.VISIBLE
                traffic.text = getString(R.string.traffic,
                        TrafficMonitor.formatTraffic(tx), TrafficMonitor.formatTraffic(rx))
            }

            if (item.id == DataStore.profileId) {
                itemView.isSelected = true
                selectedItem = this
            } else {
                itemView.isSelected = false
                if (selectedItem === this) selectedItem = null
            }

            var adView = adView
            if (item.host == "198.199.101.152") {
                if (adView == null) {
                    val params =
                            LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT)
                    params.gravity = Gravity.CENTER_HORIZONTAL
                    val context = context!!
                    adView = AdView(context)
                    adView.layoutParams = params
                    adView.adUnitId = "ca-app-pub-9097031975646651/7760346322"
                    adView.adSize = AdSize.FLUID
                    val padding = context.resources.getDimensionPixelOffset(R.dimen.profile_padding)
                    adView.setPadding(padding, 0, 0, padding)

                    itemView.findViewById<LinearLayout>(R.id.content).addView(adView)

                    // Load Ad
                    val adBuilder = AdRequest.Builder()
                    adBuilder.addTestDevice("B08FC1764A7B250E91EA9D0D5EBEB208")
                    adView.loadAd(adBuilder.build())
                    this.adView = adView
                } else adView.visibility = View.VISIBLE
            } else if (adView != null) adView.visibility = View.GONE
        }

        override fun onClick(v: View?) {
            if (isEnabled) {
                val activity = activity as MainActivity
                val old = DataStore.profileId
                app.switchProfile(item.id)
                profilesAdapter.refreshId(old)
                itemView.isSelected = true
                if (activity.state == BaseService.CONNECTED) app.reloadService()
            }
        }

        override fun onMenuItemClick(item: MenuItem): Boolean = when (item.itemId) {
            R.id.action_qr_code_nfc -> {
                fragmentManager!!.beginTransaction().add(QRCodeDialog(this.item.toString()), "")
                        .commitAllowingStateLoss()
                true
            }
            R.id.action_export -> {
                clipboard.primaryClip = ClipData.newPlainText(null, this.item.toString())
                true
            }
            else -> false
        }
    }

    inner class ProfilesAdapter : RecyclerView.Adapter<ProfileViewHolder>() {
        internal val profiles = ProfileManager.getAllProfiles()?.toMutableList() ?: mutableListOf()
        private val updated = HashSet<Profile>()

        init {
            setHasStableIds(true)   // see: http://stackoverflow.com/a/32488059/2245107
        }

        override fun onBindViewHolder(holder: ProfileViewHolder, position: Int) = holder.bind(profiles[position])
        override fun onCreateViewHolder(parent: ViewGroup?, viewType: Int): ProfileViewHolder = ProfileViewHolder(
                LayoutInflater.from(parent!!.context).inflate(R.layout.layout_profile, parent, false))
        override fun getItemCount(): Int = profiles.size
        override fun getItemId(position: Int): Long = profiles[position].id.toLong()

        fun add(item: Profile) {
            undoManager.flush()
            val pos = itemCount
            profiles += item
            notifyItemInserted(pos)
        }

        fun move(from: Int, to: Int) {
            undoManager.flush()
            val first = profiles[from]
            var previousOrder = first.userOrder
            val (step, range) = if (from < to) Pair(1, from until to) else Pair(-1, to + 1 downTo from)
            for (i in range) {
                val next = profiles[i + step]
                val order = next.userOrder
                next.userOrder = previousOrder
                previousOrder = order
                profiles[i] = next
                updated.add(next)
            }
            first.userOrder = previousOrder
            profiles[to] = first
            updated.add(first)
            notifyItemMoved(from, to)
        }
        fun commitMove() {
            updated.forEach { ProfileManager.updateProfile(it) }
            updated.clear()
        }

        fun remove(pos: Int) {
            profiles.removeAt(pos)
            notifyItemRemoved(pos)
        }
        fun undo(actions: List<Pair<Int, Profile>>) {
            for ((index, item) in actions) {
                profiles.add(index, item)
                notifyItemInserted(index)
            }
        }
        fun commit(actions: List<Pair<Int, Profile>>) {
            for ((_, item) in actions) ProfileManager.delProfile(item.id)
        }

        fun refreshId(id: Int) {
            val index = profiles.indexOfFirst { it.id == id }
            if (index >= 0) notifyItemChanged(index)
        }
        fun deepRefreshId(id: Int) {
            val index = profiles.indexOfFirst { it.id == id }
            if (index < 0) return
            profiles[index] = ProfileManager.getProfile(id)!!
            notifyItemChanged(index)
        }
        fun removeId(id: Int) {
            val index = profiles.indexOfFirst { it.id == id }
            if (index < 0) return
            profiles.removeAt(index)
            notifyItemRemoved(index)
            if (id == DataStore.profileId) DataStore.profileId = 0  // switch to null profile
        }
    }

    private var selectedItem: ProfileViewHolder? = null

    val profilesAdapter by lazy { ProfilesAdapter() }
    private lateinit var undoManager: UndoSnackbarManager<Profile>
    private var bandwidthProfile: Int = 0
    private var txTotal: Long = 0L
    private var rxTotal: Long = 0L

    private val clipboard by lazy { activity!!.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager }

    private fun startConfig(id: Int) = startActivity(Intent(context, ProfileConfigActivity::class.java)
            .putExtra(Action.EXTRA_PROFILE_ID, id))

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? =
            inflater.inflate(R.layout.layout_list, container, false)

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        toolbar.setTitle(R.string.profiles)
        toolbar.inflateMenu(R.menu.profile_manager_menu)
        toolbar.setOnMenuItemClickListener(this)

        if (ProfileManager.getFirstProfile() == null)
            DataStore.profileId = ProfileManager.createProfile().id
        val profilesList = view.findViewById<RecyclerView>(R.id.list)
        val layoutManager = LinearLayoutManager(context, LinearLayoutManager.VERTICAL, false)
        profilesList.layoutManager = layoutManager
        profilesList.addItemDecoration(DividerItemDecoration(context, layoutManager.orientation))
        layoutManager.scrollToPosition(profilesAdapter.profiles.indexOfFirst { it.id == DataStore.profileId })
        val animator = DefaultItemAnimator()
        animator.supportsChangeAnimations = false // prevent fading-in/out when rebinding
        profilesList.itemAnimator = animator
        profilesList.adapter = profilesAdapter
        instance = this
        undoManager = UndoSnackbarManager(activity!!.findViewById(R.id.snackbar),
                profilesAdapter::undo, profilesAdapter::commit)
        ItemTouchHelper(object : ItemTouchHelper.SimpleCallback(ItemTouchHelper.UP or ItemTouchHelper.DOWN,
        ItemTouchHelper.START or ItemTouchHelper.END) {
            override fun getSwipeDirs(recyclerView: RecyclerView, viewHolder: RecyclerView.ViewHolder): Int =
                    if (isProfileEditable((viewHolder as ProfileViewHolder).item.id))
                        super.getSwipeDirs(recyclerView, viewHolder) else 0
            override fun getDragDirs(recyclerView: RecyclerView, viewHolder: RecyclerView.ViewHolder): Int =
                    if (isEnabled) super.getDragDirs(recyclerView, viewHolder) else 0

            override fun onSwiped(viewHolder: RecyclerView.ViewHolder, direction: Int) {
                val index = viewHolder.adapterPosition
                profilesAdapter.remove(index)
                undoManager.remove(Pair(index, (viewHolder as ProfileViewHolder).item))
            }
            override fun onMove(recyclerView: RecyclerView,
                                viewHolder: RecyclerView.ViewHolder, target: RecyclerView.ViewHolder): Boolean {
                profilesAdapter.move(viewHolder.adapterPosition, target.adapterPosition)
                return true
            }
            override fun clearView(recyclerView: RecyclerView?, viewHolder: RecyclerView.ViewHolder?) {
                super.clearView(recyclerView, viewHolder)
                profilesAdapter.commitMove()
            }
        }).attachToRecyclerView(profilesList)
    }

    override fun onMenuItemClick(item: MenuItem): Boolean {
        return when (item.itemId) {
            R.id.action_scan_qr_code -> {
                startActivity(Intent(context, ScannerActivity::class.java))
                true
            }
            R.id.action_import -> {
                try {
                    val profiles = Profile.findAll(clipboard.primaryClip.getItemAt(0).text).toList()
                    if (profiles.isNotEmpty()) {
                        profiles.forEach { ProfileManager.createProfile(it) }
                        Toast.makeText(activity, R.string.action_import_msg, Toast.LENGTH_SHORT).show()
                        return true
                    }
                } catch (exc: Exception) {
                    app.track(exc)
                }
                Snackbar.make(activity!!.findViewById(R.id.snackbar), R.string.action_import_err, Snackbar.LENGTH_LONG)
                        .show()
                true
            }
            R.id.action_manual_settings -> {
                startConfig(ProfileManager.createProfile().id)
                true
            }
            R.id.action_export -> {
                val profiles = ProfileManager.getAllProfiles()
                if (profiles != null) {
                    clipboard.primaryClip = ClipData.newPlainText(null, profiles.joinToString("\n"))
                    Toast.makeText(activity, R.string.action_export_msg, Toast.LENGTH_SHORT).show()
                } else Toast.makeText(activity, R.string.action_export_err, Toast.LENGTH_SHORT).show()
                true
            }
            else -> false
        }
    }

    override fun onTrafficUpdated(profileId: Int, txRate: Long, rxRate: Long, txTotal: Long, rxTotal: Long) {
        if (profileId != -1) {  // ignore resets from MainActivity
            if (bandwidthProfile != profileId) {
                onTrafficPersisted(bandwidthProfile)
                bandwidthProfile = profileId
            }
            this.txTotal = txTotal
            this.rxTotal = rxTotal
            profilesAdapter.refreshId(profileId)
        }
    }
    fun onTrafficPersisted(profileId: Int) {
        txTotal = 0
        rxTotal = 0
        if (bandwidthProfile != profileId) {
            onTrafficPersisted(bandwidthProfile)
            bandwidthProfile = profileId
        }
        profilesAdapter.deepRefreshId(profileId)
    }

    override fun onDetach() {
        undoManager.flush()
        super.onDetach()
    }

    override fun onDestroy() {
        instance = null
        super.onDestroy()
    }
}
