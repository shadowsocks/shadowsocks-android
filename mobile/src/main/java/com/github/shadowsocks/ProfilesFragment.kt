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
import android.app.Activity
import android.content.*
import android.os.Bundle
import android.text.format.Formatter
import android.util.LongSparseArray
import android.view.*
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.TextView
import androidx.appcompat.widget.PopupMenu
import androidx.appcompat.widget.Toolbar
import androidx.appcompat.widget.TooltipCompat
import androidx.core.content.getSystemService
import androidx.core.os.bundleOf
import androidx.fragment.app.DialogFragment
import androidx.recyclerview.widget.*
import com.github.shadowsocks.aidl.TrafficStats
import com.github.shadowsocks.bg.BaseService
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.database.ProfileManager
import com.github.shadowsocks.plugin.PluginConfiguration
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.utils.Action
import com.github.shadowsocks.utils.datas
import com.github.shadowsocks.utils.printLog
import com.github.shadowsocks.utils.readableMessage
import com.github.shadowsocks.widget.UndoSnackbarManager
import com.google.android.gms.ads.AdRequest
import com.google.android.gms.ads.AdSize
import com.google.android.gms.ads.AdView
import net.glxn.qrgen.android.QRCode

class ProfilesFragment : ToolbarFragment(), Toolbar.OnMenuItemClickListener {
    companion object {
        /**
         * used for callback from stateChanged from MainActivity
         */
        var instance: ProfilesFragment? = null

        private const val KEY_URL = "com.github.shadowsocks.QRCodeDialog.KEY_URL"
        private const val REQUEST_IMPORT_PROFILES = 1
        private const val REQUEST_REPLACE_PROFILES = 3
        private const val REQUEST_EXPORT_PROFILES = 2
    }

    /**
     * Is ProfilesFragment editable at all.
     */
    private val isEnabled get() = (activity as MainActivity).state.let { it.canStop || it == BaseService.State.Stopped }
    private fun isProfileEditable(id: Long) =
            (activity as MainActivity).state == BaseService.State.Stopped || id !in Core.activeProfileIds
    private var isAdLoaded = false

    @SuppressLint("ValidFragment")
    class QRCodeDialog() : DialogFragment() {
        constructor(url: String) : this() {
            arguments = bundleOf(Pair(KEY_URL, url))
        }

        override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
            val image = ImageView(context)
            image.layoutParams = LinearLayout.LayoutParams(-1, -1)
            val size = resources.getDimensionPixelSize(R.dimen.qr_code_size)
            image.setImageBitmap((QRCode.from(arguments?.getString(KEY_URL)!!).withSize(size, size) as QRCode).bitmap())
            return image
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
            edit.setOnClickListener {
                item = ProfileManager.getProfile(item.id)!!
                startConfig(item)
            }
            TooltipCompat.setTooltipText(edit, edit.contentDescription)
            itemView.setOnClickListener(this)
            val share = itemView.findViewById<View>(R.id.share)
            share.setOnClickListener {
                val popup = PopupMenu(requireContext(), share)
                popup.menuInflater.inflate(R.menu.profile_share_popup, popup.menu)
                popup.setOnMenuItemClickListener(this)
                popup.show()
            }
            TooltipCompat.setTooltipText(share, share.contentDescription)
        }

        fun attach() {
            if (!isAdLoaded && item.host == "198.199.101.152") {
                if (this.adView == null) {
                    val params = LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                            AdSize.SMART_BANNER.getHeightInPixels(context))
                    params.gravity = Gravity.CENTER_HORIZONTAL

                    var adView = AdView(context)
                    adView.layoutParams = params
                    adView.adUnitId = "ca-app-pub-9097031975646651/7760346322"
                    adView.adSize = AdSize.SMART_BANNER

                    itemView.findViewById<LinearLayout>(R.id.content).addView(adView)

                    // Load Ad
                    val adBuilder = AdRequest.Builder()
                    adBuilder.addTestDevice("B08FC1764A7B250E91EA9D0D5EBEB208")
                    adBuilder.addTestDevice("7509D18EB8AF82F915874FEF53877A64")
                    adView.loadAd(adBuilder.build())
                    this.adView = adView
                } else this.adView?.visibility = View.VISIBLE

                isAdLoaded = true
            } else this.adView?.visibility = View.GONE
        }

        fun detach() {
            if (this.adView?.visibility == View.VISIBLE) {
                isAdLoaded = false
                this.adView?.visibility == View.GONE
            }
        }

        fun bind(item: Profile) {
            this.item = item
            val editable = isProfileEditable(item.id)
            edit.isEnabled = editable
            edit.alpha = if (editable) 1F else .5F
            var tx = item.tx
            var rx = item.rx
            statsCache[item.id]?.apply {
                tx += txTotal
                rx += rxTotal
            }
            text1.text = item.formattedName
            text2.text = ArrayList<String>().apply {
                if (!item.name.isNullOrEmpty()) this += item.formattedAddress
                val id = PluginConfiguration(item.plugin ?: "").selected
                if (id.isNotEmpty()) this += getString(R.string.profile_plugin, id)
            }.joinToString("\n")
            val context = requireContext()
            traffic.text = if (tx <= 0 && rx <= 0) null else getString(R.string.traffic,
                    Formatter.formatFileSize(context, tx), Formatter.formatFileSize(context, rx))

            if (item.id == DataStore.profileId) {
                itemView.isSelected = true
                selectedItem = this
            } else {
                itemView.isSelected = false
                if (selectedItem === this) selectedItem = null
            }
        }

        override fun onClick(v: View?) {
            if (isEnabled) {
                val activity = activity as MainActivity
                val old = DataStore.profileId
                Core.switchProfile(item.id)
                profilesAdapter.refreshId(old)
                itemView.isSelected = true
                if (activity.state.canStop) Core.reloadService()
            }
        }

        override fun onMenuItemClick(item: MenuItem): Boolean = when (item.itemId) {
            R.id.action_qr_code -> {
                requireFragmentManager().beginTransaction().add(QRCodeDialog(this.item.toString()), "")
                        .commitAllowingStateLoss()
                true
            }
            R.id.action_export_clipboard -> {
                clipboard.setPrimaryClip(ClipData.newPlainText(null, this.item.toString()))
                true
            }
            else -> false
        }
    }

    inner class ProfilesAdapter : RecyclerView.Adapter<ProfileViewHolder>(), ProfileManager.Listener {
        internal val profiles = ProfileManager.getAllProfiles()?.toMutableList() ?: mutableListOf()
        private val updated = HashSet<Profile>()

        init {
            setHasStableIds(true)   // see: http://stackoverflow.com/a/32488059/2245107
        }

        override fun onViewAttachedToWindow(holder: ProfileViewHolder)   = holder.attach()
        override fun onViewDetachedFromWindow(holder: ProfileViewHolder) = holder.detach()
        override fun onBindViewHolder(holder: ProfileViewHolder, position: Int) = holder.bind(profiles[position])
        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ProfileViewHolder = ProfileViewHolder(
                LayoutInflater.from(parent.context).inflate(R.layout.layout_profile, parent, false))
        override fun getItemCount(): Int = profiles.size
        override fun getItemId(position: Int): Long = profiles[position].id

        override fun onAdd(profile: Profile) {
            undoManager.flush()
            val pos = itemCount
            profiles += profile
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

        fun refreshId(id: Long) {
            val index = profiles.indexOfFirst { it.id == id }
            if (index >= 0) notifyItemChanged(index)
        }
        fun deepRefreshId(id: Long) {
            val index = profiles.indexOfFirst { it.id == id }
            if (index < 0) return
            profiles[index] = ProfileManager.getProfile(id)!!
            notifyItemChanged(index)
        }
        override fun onRemove(profileId: Long) {
            val index = profiles.indexOfFirst { it.id == profileId }
            if (index < 0) return
            profiles.removeAt(index)
            notifyItemRemoved(index)
            if (profileId == DataStore.profileId) DataStore.profileId = 0   // switch to null profile
        }

        override fun onCleared() {
            profiles.clear()
            notifyDataSetChanged()
        }
    }

    private var selectedItem: ProfileViewHolder? = null

    val profilesAdapter by lazy { ProfilesAdapter() }
    private lateinit var undoManager: UndoSnackbarManager<Profile>
    private val statsCache = LongSparseArray<TrafficStats>()

    private val clipboard by lazy { requireContext().getSystemService<ClipboardManager>()!! }

    private fun startConfig(profile: Profile) {
        profile.serialize()
        startActivity(Intent(context, ProfileConfigActivity::class.java).putExtra(Action.EXTRA_PROFILE_ID, profile.id))
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? =
            inflater.inflate(R.layout.layout_list, container, false)

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        toolbar.setTitle(R.string.profiles)
        toolbar.inflateMenu(R.menu.profile_manager_menu)
        toolbar.setOnMenuItemClickListener(this)

        isAdLoaded = false

        ProfileManager.ensureNotEmpty()
        val profilesList = view.findViewById<RecyclerView>(R.id.list)
        val layoutManager = LinearLayoutManager(context, RecyclerView.VERTICAL, false)
        profilesList.layoutManager = layoutManager
        profilesList.addItemDecoration(DividerItemDecoration(context, layoutManager.orientation))
        layoutManager.scrollToPosition(profilesAdapter.profiles.indexOfFirst { it.id == DataStore.profileId })
        val animator = DefaultItemAnimator()
        animator.supportsChangeAnimations = false // prevent fading-in/out when rebinding
        profilesList.itemAnimator = animator
        profilesList.adapter = profilesAdapter
        instance = this
        ProfileManager.listener = profilesAdapter
        undoManager = UndoSnackbarManager(activity as MainActivity, profilesAdapter::undo, profilesAdapter::commit)
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
            override fun clearView(recyclerView: RecyclerView, viewHolder: RecyclerView.ViewHolder) {
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
            R.id.action_import_clipboard -> {
                try {
                    val profiles = Profile.findAllUrls(
                            clipboard.primaryClip!!.getItemAt(0).text,
                            Core.currentProfile?.first
                    ).toList()
                    if (profiles.isNotEmpty()) {
                        profiles.forEach { ProfileManager.createProfile(it) }
                        (activity as MainActivity).snackbar().setText(R.string.action_import_msg).show()
                        return true
                    }
                } catch (exc: Exception) {
                    exc.printStackTrace()
                }
                (activity as MainActivity).snackbar().setText(R.string.action_import_err).show()
                true
            }
            R.id.action_import_file -> {
                startFilesForResult(Intent(Intent.ACTION_GET_CONTENT).apply {
                    type = "application/*"
                    putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true)
                    putExtra(Intent.EXTRA_MIME_TYPES, arrayOf("application/*", "text/*"))
                }, REQUEST_IMPORT_PROFILES)
                true
            }
            R.id.action_replace_file -> {
                startFilesForResult(Intent(Intent.ACTION_GET_CONTENT).apply {
                    type = "application/*"
                    putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true)
                    putExtra(Intent.EXTRA_MIME_TYPES, arrayOf("application/*", "text/*"))
                }, REQUEST_REPLACE_PROFILES)
                true
            }
            R.id.action_manual_settings -> {
                startConfig(ProfileManager.createProfile(
                        Profile().also { Core.currentProfile?.first?.copyFeatureSettingsTo(it) }))
                true
            }
            R.id.action_export_clipboard -> {
                val profiles = ProfileManager.getAllProfiles()
                (activity as MainActivity).snackbar().setText(if (profiles != null) {
                    clipboard.setPrimaryClip(ClipData.newPlainText(null, profiles.joinToString("\n")))
                    R.string.action_export_msg
                } else R.string.action_export_err).show()
                true
            }
            R.id.action_export_file -> {
                startFilesForResult(Intent(Intent.ACTION_CREATE_DOCUMENT).apply {
                    type = "application/json"
                    putExtra(Intent.EXTRA_TITLE, "profiles.json")   // optional title that can be edited
                }, REQUEST_EXPORT_PROFILES)
                true
            }
            else -> false
        }
    }

    private fun startFilesForResult(intent: Intent, requestCode: Int) {
        try {
            startActivityForResult(intent.addCategory(Intent.CATEGORY_OPENABLE), requestCode)
            return
        } catch (_: ActivityNotFoundException) { } catch (_: SecurityException) { }
        (activity as MainActivity).snackbar(getString(R.string.file_manager_missing)).show()
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        if (resultCode != Activity.RESULT_OK) super.onActivityResult(requestCode, resultCode, data)
        else when (requestCode) {
            REQUEST_IMPORT_PROFILES -> {
                val activity = activity as MainActivity
                try {
                    ProfileManager.createProfilesFromJson(data!!.datas.asSequence().map {
                        activity.contentResolver.openInputStream(it)
                    }.filterNotNull())
                } catch (e: Exception) {
                    activity.snackbar(e.readableMessage).show()
                }
            }
            REQUEST_REPLACE_PROFILES -> {
                val activity = activity as MainActivity
                try {
                    ProfileManager.createProfilesFromJson(data!!.datas.asSequence().map {
                        activity.contentResolver.openInputStream(it)
                    }.filterNotNull(), true)
                } catch (e: Exception) {
                    activity.snackbar(e.readableMessage).show()
                }
            }
            REQUEST_EXPORT_PROFILES -> {
                val profiles = ProfileManager.serializeToJson()
                if (profiles != null) try {
                    requireContext().contentResolver.openOutputStream(data?.data!!)!!.bufferedWriter().use {
                        it.write(profiles.toString(2))
                    }
                } catch (e: Exception) {
                    printLog(e)
                    (activity as MainActivity).snackbar(e.readableMessage).show()
                }
            }
            else -> super.onActivityResult(requestCode, resultCode, data)
        }
    }

    override fun onTrafficUpdated(profileId: Long, stats: TrafficStats) {
        if (profileId != 0L) {  // ignore aggregate stats
            statsCache.put(profileId, stats)
            profilesAdapter.refreshId(profileId)
        }
    }
    fun onTrafficPersisted(profileId: Long) {
        statsCache.remove(profileId)
        profilesAdapter.deepRefreshId(profileId)
    }

    override fun onDestroyView() {
        undoManager.flush()
        super.onDestroyView()
    }

    override fun onDestroy() {
        instance = null
        ProfileManager.listener = null
        super.onDestroy()
    }
}
