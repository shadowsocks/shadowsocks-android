package com.github.shadowsocks.controllers

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Intent
import android.nfc.NdefMessage
import android.nfc.NdefRecord
import android.nfc.NfcAdapter
import android.os.Bundle
import android.support.design.widget.Snackbar
import android.support.v4.widget.NestedScrollView
import android.support.v7.widget.*
import android.support.v7.widget.helper.ItemTouchHelper
import android.text.format.Formatter
import android.view.*
import android.widget.FrameLayout
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.TextView
import com.bluelinelabs.conductor.Controller
import com.bluelinelabs.conductor.ControllerChangeHandler
import com.bluelinelabs.conductor.ControllerChangeType
import com.bluelinelabs.conductor.RouterTransaction
import com.bluelinelabs.conductor.changehandler.FadeChangeHandler
import com.github.shadowsocks.*
import com.github.shadowsocks.bg.BaseService
import com.github.shadowsocks.controllers.base.BaseController
import com.github.shadowsocks.controllers.changehandler.ScaleFadeChangeHandler
import com.github.shadowsocks.controllers.widget.ElasticDragDismissFrameLayout
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.database.ProfileManager
import com.github.shadowsocks.plugin.PluginConfiguration
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.utils.Action
import com.github.shadowsocks.utils.systemService
import com.github.shadowsocks.widget.UndoSnackbarManager
import com.google.android.gms.ads.AdRequest
import com.google.android.gms.ads.AdSize
import com.google.android.gms.ads.AdView
import net.glxn.qrgen.android.QRCode

class ProfilesController : BaseController() {

    companion object {
        const val TAG = "ProfilesController"
        var instance: ProfilesController? = null
        private const val KEY_URL = "com.github.shadowsocks.QRCodeDialog.KEY_URL"
    }
    private val url get() = args.getString(KEY_URL)

    init {
        setHasOptionsMenu(true)
    }

    /**
     * Is ProfilesFragment editable at all.
     */
    private val isEnabled get() = when ((activity as MainActivity).state) {
        BaseService.CONNECTED, BaseService.STOPPED -> true
        else -> false
    }
    private fun isProfileEditable(id: Long) = when ((activity as MainActivity).state) {
        BaseService.CONNECTED -> id != DataStore.profileId
        BaseService.STOPPED -> true
        else -> false
    }
    private var selectedItem: ProfileViewHolder? = null
    val profilesAdapter by lazy { ProfilesAdapter() }
    private lateinit var undoManager: UndoSnackbarManager<Profile>
    private var bandwidthProfile = 0L
    private var txTotal: Long = 0L
    private var rxTotal: Long = 0L

    private val clipboard by lazy { activity!!.baseContext.systemService<ClipboardManager>() }

    private fun startConfig(profile: Profile) {
        profile.serialize()
        startActivity(Intent(activity, ProfileConfigActivity::class.java).putExtra(Action.EXTRA_PROFILE_ID, profile.id))
    }

    override fun inflateView(inflater: LayoutInflater, container: ViewGroup): View =
            inflater.inflate(R.layout.layout_list, container, false)

    override fun onViewBound(view: View) {
        super.onViewBound(view)
        if (!ProfileManager.isNotEmpty()) DataStore.profileId = ProfileManager.createProfile().id
        val profilesList = view.findViewById<RecyclerView>(R.id.list)
        val layoutManager = LinearLayoutManager(activity, LinearLayoutManager.VERTICAL, false)
        profilesList.layoutManager = layoutManager
        profilesList.addItemDecoration(DividerItemDecoration(activity, layoutManager.orientation))
        layoutManager.scrollToPosition(profilesAdapter.profiles.indexOfFirst { it.id == DataStore.profileId })
        val animator = DefaultItemAnimator()
        animator.supportsChangeAnimations = false // prevent fading-in/out when rebinding
        profilesList.itemAnimator = animator
        profilesList.adapter = profilesAdapter
        instance = this
        undoManager = UndoSnackbarManager(activity!!.findViewById(R.id.snackbar), profilesAdapter::undo, profilesAdapter::commit)
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

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        super.onCreateOptionsMenu(menu, inflater)
        menu.clear()
        inflater.inflate(R.menu.profile_manager_menu, menu)
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        when (item.itemId) {
            R.id.action_scan_qr_code -> startActivity(Intent(this.activity, ScannerActivity::class.java))
            R.id.action_import -> {
                try {
                    val profiles = Profile.findAll(clipboard.primaryClip!!.getItemAt(0).text).toList()
                    if (profiles.isNotEmpty()) {
                        profiles.forEach { ProfileManager.createProfile(it) }
                        Snackbar.make(activity!!.findViewById(R.id.snackbar), R.string.action_import_msg,
                                Snackbar.LENGTH_LONG).show()
                        return true
                    }
                } catch (exc: Exception) {
                    exc.printStackTrace()
                }
                Snackbar.make(activity!!.findViewById(R.id.snackbar), R.string.action_import_err,
                        Snackbar.LENGTH_LONG).show()
            }
            R.id.action_manual_settings -> {
                startConfig(ProfileManager.createProfile())
            }
            R.id.action_export -> {
                val profiles = ProfileManager.getAllProfiles()
                Snackbar.make(activity!!.findViewById(R.id.snackbar), if (profiles != null) {
                    clipboard.primaryClip = ClipData.newPlainText(null, profiles.joinToString("\n"))
                    R.string.action_export_msg
                } else R.string.action_export_err, Snackbar.LENGTH_LONG).show()
            }
        }
        return super.onOptionsItemSelected(item)
    }

    override fun onAttach(view: View) {
        (activity as MainActivity).toolbar.setTitle(R.string.profiles)
        super.onAttach(view)
    }

    override fun onChangeStarted(changeHandler: ControllerChangeHandler, changeType: ControllerChangeType) {
        setOptionsMenuHidden(!changeType.isEnter)
        if (changeType.isEnter) {
            (activity as MainActivity).toolbar.setTitle(R.string.profiles)
        }
    }

    override fun onDetach(view: View) {
        undoManager.flush()
        super.onDetach(view)
    }

    override fun onDestroy() {
        instance = null
        super.onDestroy()
    }

    override fun onTrafficUpdated(profileId: Long, txRate: Long, rxRate: Long, txTotal: Long, rxTotal: Long) {
        if (profileId != -1L) { // ignore resets from MainActivity
            if (bandwidthProfile != profileId) {
                onTrafficPersisted(bandwidthProfile)
                bandwidthProfile = profileId
            }
            this.txTotal = txTotal
            this.rxTotal = rxTotal
            profilesAdapter.refreshId(profileId)
        }
    }
    fun onTrafficPersisted(profileId: Long) {
        txTotal = 0
        rxTotal = 0
        if (bandwidthProfile != profileId) {
            onTrafficPersisted(bandwidthProfile)
            bandwidthProfile = profileId
        }
        profilesAdapter.deepRefreshId(profileId)
    }

    class QRCodeDialog(args: Bundle) : Controller(args) {

        constructor(url: String) : this(Bundle().apply {
            putString(KEY_URL, url)
        })

        private val url = args.getString(KEY_URL)
        private val nfcShareItem by lazy { url.toByteArray() }
        private var adapter: NfcAdapter? = null

        private val dragDismissListener = object : ElasticDragDismissFrameLayout.ElasticDragDismissCallback() {
            override fun onDragDismissed() {
                overridePopHandler(ScaleFadeChangeHandler())
                router.popController(this@QRCodeDialog)
            }
        }

        override fun onCreateView(inflater: LayoutInflater, container: ViewGroup): View {
            val image = ImageView(activity)
            val layout = ElasticDragDismissFrameLayout(activity!!)
            layout.layoutParams = FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT)
            val scrollView = NestedScrollView(activity!!)
            val lp = FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
            lp.gravity = Gravity.CENTER
            scrollView.layoutParams = lp
            image.layoutParams = lp
            val size = resources!!.getDimensionPixelSize(R.dimen.qr_code_size)
            image.setImageBitmap((QRCode.from(url).withSize(size, size) as QRCode).bitmap())
            scrollView.addView(image)
            layout.addView(scrollView)
            layout.addListener(dragDismissListener)
            return layout
        }

        override fun onDestroyView(view: View) {
            super.onDestroyView(view)
            (view as ElasticDragDismissFrameLayout).removeListener(dragDismissListener)
        }
        override fun onAttach(view: View) {
            super.onAttach(view)
            val adapter = NfcAdapter.getDefaultAdapter(activity)
            adapter?.setNdefPushMessage(NdefMessage(arrayOf(
                    NdefRecord(NdefRecord.TNF_ABSOLUTE_URI, nfcShareItem, byteArrayOf(), nfcShareItem))), activity)
            this.adapter = adapter
        }

        override fun onDetach(view: View) {
            super.onDetach(view)
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
            edit.setOnClickListener {
                item = ProfileManager.getProfile(item.id)!!
                startConfig(item)
            }
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
            if (id.isNotEmpty()) t2 += App.app.getString(R.string.profile_plugin, id)
            if (t2.isEmpty()) text2.visibility = View.GONE else {
                text2.visibility = View.VISIBLE
                text2.text = t2.joinToString("\n")
            }
            val context = activity?.baseContext
            if (tx <= 0 && rx <= 0) traffic.visibility = View.GONE else {
                traffic.visibility = View.VISIBLE
                traffic.text = resources!!.getString(R.string.traffic,
                        Formatter.formatFileSize(context, tx), Formatter.formatFileSize(context, rx))
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
                    val params = LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT)
                    params.gravity = Gravity.CENTER_HORIZONTAL
                    adView = AdView(context)
                    adView.layoutParams = params
                    adView.adUnitId = "ca-app-pub-9097031975646651/7760346322"
                    adView.adSize = AdSize.FLUID
                    val padding = resources!!.getDimensionPixelOffset(R.dimen.profile_padding)
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
                App.app.switchProfile(item.id)
                profilesAdapter.refreshId(old)
                itemView.isSelected = true
                if (activity.state == BaseService.CONNECTED) App.app.reloadService()
            }
        }

        override fun onMenuItemClick(item: MenuItem): Boolean = when (item.itemId) {
            R.id.action_qr_code_nfc -> {
                router.pushController(RouterTransaction.with(QRCodeDialog(this.item.toString()))
                        .pushChangeHandler(FadeChangeHandler(false))
                        .popChangeHandler(FadeChangeHandler()))
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
        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ProfileViewHolder = ProfileViewHolder(
                LayoutInflater.from(parent.context).inflate(R.layout.layout_profile, parent, false))
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
        fun removeId(id: Long) {
            val index = profiles.indexOfFirst { it.id == id }
            if (index < 0) return
            profiles.removeAt(index)
            notifyItemRemoved(index)
            if (id == DataStore.profileId) DataStore.profileId = 0  // switch to null profile
        }
    }
}