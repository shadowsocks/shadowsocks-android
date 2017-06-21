/*******************************************************************************/
/*                                                                             */
/*  Copyright (C) 2017 by Max Lv <max.c.lv@gmail.com>                          */
/*  Copyright (C) 2017 by Mygod Studio <contact-shadowsocks-android@mygod.be>  */
/*                                                                             */
/*  This program is free software: you can redistribute it and/or modify       */
/*  it under the terms of the GNU General Public License as published by       */
/*  the Free Software Foundation, either version 3 of the License, or          */
/*  (at your option) any later version.                                        */
/*                                                                             */
/*  This program is distributed in the hope that it will be useful,            */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of             */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              */
/*  GNU General Public License for more details.                               */
/*                                                                             */
/*  You should have received a copy of the GNU General Public License          */
/*  along with this program. If not, see <http://www.gnu.org/licenses/>.       */
/*                                                                             */
/*******************************************************************************/

package com.github.shadowsocks

import java.util.GregorianCalendar

import android.app.Activity
import android.content._
import android.net.Uri
import android.os.Bundle
import android.support.design.widget.Snackbar
import android.support.v7.widget.RecyclerView.ViewHolder
import android.support.v7.widget._
import android.support.v7.widget.helper.ItemTouchHelper
import android.support.v7.widget.helper.ItemTouchHelper.SimpleCallback
import android.text.TextUtils
import android.view.View.OnLongClickListener
import android.view._
import android.widget.{LinearLayout, PopupMenu, TextView, Toast}
import com.github.shadowsocks.ShadowsocksApplication.app
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.plugin.PluginConfiguration
import com.github.shadowsocks.utils._
import com.github.shadowsocks.widget.UndoSnackbarManager
import com.google.android.gms.ads.{AdRequest, AdSize, NativeExpressAdView}

import scala.collection.mutable.{ArrayBuffer, ListBuffer}
import scala.util.Random

object ProfilesFragment {
  var instance: ProfilesFragment = _  // used for callback from ProfileManager and stateChanged from MainActivity

  private final val REQUEST_SCAN_QR_CODE = 0
}

final class ProfilesFragment extends ToolbarFragment with Toolbar.OnMenuItemClickListener {
  import ProfilesFragment._

  private val cardButtonLongClickListener: OnLongClickListener = view => {
    Utils.positionToast(Toast.makeText(getActivity, view.getContentDescription, Toast.LENGTH_SHORT), view,
      getActivity.getWindow, 0, getResources.getDimensionPixelOffset(R.dimen.margin_small)).show()
    true
  }

  /**
    * Is ProfilesFragment editable at all.
    */
  private def isEnabled = getActivity.asInstanceOf[MainActivity].state match {
    case State.CONNECTED | State.STOPPED => true
    case _ => false
  }
  private def isProfileEditable(id: => Int) = getActivity.asInstanceOf[MainActivity].state match {
    case State.CONNECTED => id != app.profileId
    case State.STOPPED => true
    case _ => false
  }

  final class ProfileViewHolder(view: View) extends RecyclerView.ViewHolder(view)
    with View.OnClickListener with PopupMenu.OnMenuItemClickListener {

    var item: Profile = _

    private val text1 = itemView.findViewById(android.R.id.text1).asInstanceOf[TextView]
    private val text2 = itemView.findViewById(android.R.id.text2).asInstanceOf[TextView]
    private val traffic = itemView.findViewById(R.id.traffic).asInstanceOf[TextView]
    private val edit = itemView.findViewById(R.id.edit)
    edit.setOnClickListener(_ => startConfig(item.id))
    edit.setOnLongClickListener(cardButtonLongClickListener)
    itemView.setOnClickListener(this)
    // it will not take effect unless set in code
    itemView.findViewById(R.id.indicator).setBackgroundResource(R.drawable.background_profile)

    private var adView: NativeExpressAdView = _

    {
      val share = itemView.findViewById(R.id.share)
      share.setOnClickListener(_ => {
        val popup = new PopupMenu(getActivity, share)
        popup.getMenuInflater.inflate(R.menu.profile_share_popup, popup.getMenu)
        popup.setOnMenuItemClickListener(this)
        popup.show()
      })
      share.setOnLongClickListener(cardButtonLongClickListener)
    }

    def bind(item: Profile) {
      this.item = item
      val editable = isProfileEditable(item.id)
      edit.setEnabled(editable)
      edit.setAlpha(if (editable) 1 else 0.5F)
      var tx = item.tx
      var rx = item.rx
      if (item.id == bandwidthProfile) {
        tx += txTotal
        rx += rxTotal
      }
      text1.setText(item.getName)
      val t2 = new ListBuffer[String]
      if (!item.nameIsEmpty) t2 += item.formattedAddress
      new PluginConfiguration(item.plugin).selected match {
        case "" =>
        case id => t2 += app.getString(R.string.profile_plugin, id)
      }
      if (t2.isEmpty) text2.setVisibility(View.GONE) else {
        text2.setVisibility(View.VISIBLE)
        text2.setText(t2.mkString("\n"))
      }
      if (tx <= 0 && rx <= 0) traffic.setVisibility(View.GONE) else {
        traffic.setVisibility(View.VISIBLE)
        traffic.setText(getString(R.string.stat_profiles,
          TrafficMonitor.formatTraffic(tx), TrafficMonitor.formatTraffic(rx)))
      }

      if (item.id == app.profileId) {
        itemView.setSelected(true)
        selectedItem = this
      } else {
        itemView.setSelected(false)
        if (selectedItem eq this) selectedItem = null
      }

      if (item.host == "198.199.101.152") {
        if (adView == null) {
          val params =
            new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT)
          params.gravity = Gravity.CENTER_HORIZONTAL
          params.setMargins(0, getResources.getDimensionPixelOffset(R.dimen.margin_small), 0, 0)
          adView = new NativeExpressAdView(getActivity)
          adView.setLayoutParams(params)
          adView.setAdUnitId("ca-app-pub-9097031975646651/5224027521")
          adView.setAdSize(new AdSize(328, 132))
          itemView.findViewById(R.id.content).asInstanceOf[LinearLayout].addView(adView)

          // Demographics
          val random = new Random()
          val adBuilder = new AdRequest.Builder()
          adBuilder.setGender(AdRequest.GENDER_MALE)
          val year = 1975 + random.nextInt(40)
          val month = 1 + random.nextInt(12)
          val day = random.nextInt(28)
          adBuilder.setBirthday(new GregorianCalendar(year, month, day).getTime)

          // Load Ad
          adView.loadAd(adBuilder.build())
        } else adView.setVisibility(View.VISIBLE)
      } else if (adView != null) adView.setVisibility(View.GONE)
    }

    def onClick(v: View): Unit = if (isEnabled) {
      val activity = getActivity.asInstanceOf[MainActivity]
      val old = app.profileId
      app.switchProfile(item.id)
      profilesAdapter.refreshId(old)
      itemView.setSelected(true)
      if (activity.state == State.CONNECTED) activity.bgService.use(item.id)  // reconnect to new profile
    }

    override def onMenuItemClick(menu: MenuItem): Boolean = menu.getItemId match {
      case R.id.action_qr_code_nfc =>
        getFragmentManager.beginTransaction().add(new QRCodeDialog(item.toString), "").commitAllowingStateLoss()
        true
      case R.id.action_export =>
        clipboard.setPrimaryClip(ClipData.newPlainText(null, item.toString))
        true
      case _ => false
    }
  }

  final class ProfilesAdapter extends RecyclerView.Adapter[ProfileViewHolder] {
    var profiles = new ArrayBuffer[Profile]
    profiles ++= app.profileManager.getAllProfiles.getOrElse(List.empty[Profile])

    def getItemCount: Int = profiles.length

    def onBindViewHolder(vh: ProfileViewHolder, i: Int): Unit = vh.bind(profiles(i))

    def onCreateViewHolder(vg: ViewGroup, i: Int) =
      new ProfileViewHolder(LayoutInflater.from(vg.getContext).inflate(R.layout.layout_profile, vg, false))

    setHasStableIds(true) // Reason: http://stackoverflow.com/a/32488059/2245107
    override def getItemId(position: Int): Long = profiles(position).id

    def add(item: Profile) {
      undoManager.flush()
      val pos = getItemCount
      profiles += item
      notifyItemInserted(pos)
    }

    def move(from: Int, to: Int) {
      undoManager.flush()
      val step = if (from < to) 1 else -1
      val first = profiles(from)
      var previousOrder = profiles(from).userOrder
      for (i <- from until to by step) {
        val next = profiles(i + step)
        val order = next.userOrder
        next.userOrder = previousOrder
        previousOrder = order
        profiles(i) = next
        app.profileManager.updateProfile(next)
      }
      first.userOrder = previousOrder
      profiles(to) = first
      app.profileManager.updateProfile(first)
      notifyItemMoved(from, to)
    }

    def remove(pos: Int) {
      profiles.remove(pos)
      notifyItemRemoved(pos)
    }
    def undo(actions: Iterator[(Int, Profile)]): Unit = for ((index, item) <- actions) {
      profiles.insert(index, item)
      notifyItemInserted(index)
    }
    def commit(actions: Iterator[(Int, Profile)]): Unit =
      for ((_, item) <- actions) app.profileManager.delProfile(item.id)

    def refreshId(id: Int) {
      val index = profiles.indexWhere(_.id == id)
      if (index >= 0) notifyItemChanged(index)
    }
    def deepRefreshId(id: Int) {
      val index = profiles.indexWhere(_.id == id)
      if (index >= 0) {
        profiles(index) = app.profileManager.getProfile(id).get
        notifyItemChanged(index)
      }
    }
    def removeId(id: Int) {
      val index = profiles.indexWhere(_.id == id)
      if (index >= 0) {
        profiles.remove(index)
        notifyItemRemoved(index)
        if (id == app.profileId) app.profileId(0) // switch to null profile
      }
    }
  }

  private var selectedItem: ProfileViewHolder = _

  lazy val profilesAdapter = new ProfilesAdapter
  private var undoManager: UndoSnackbarManager[Profile] = _
  private var bandwidthProfile: Int = _
  private var txTotal: Long = _
  private var rxTotal: Long = _

  private lazy val clipboard = getActivity.getSystemService(Context.CLIPBOARD_SERVICE).asInstanceOf[ClipboardManager]

  private def startConfig(id: Int) = startActivity(new Intent(getActivity, classOf[ProfileConfigActivity])
    .putExtra(Action.EXTRA_PROFILE_ID, id))

  override def onCreateView(inflater: LayoutInflater, container: ViewGroup, savedInstanceState: Bundle): View =
    inflater.inflate(R.layout.layout_list, container, false)

  override def onViewCreated(view: View, savedInstanceState: Bundle) {
    super.onViewCreated(view, savedInstanceState)
    toolbar.setTitle(R.string.profiles)
    toolbar.inflateMenu(R.menu.profile_manager_menu)
    toolbar.setOnMenuItemClickListener(this)

    if (app.profileManager.getFirstProfile.isEmpty) app.profileId(app.profileManager.createProfile().id)
    val profilesList = view.findViewById(R.id.list).asInstanceOf[RecyclerView]
    val layoutManager = new LinearLayoutManager(getActivity, LinearLayoutManager.VERTICAL, false)
    profilesList.setLayoutManager(layoutManager)
    layoutManager.scrollToPosition(profilesAdapter.profiles.zipWithIndex.collectFirst {
      case (profile, i) if profile.id == app.profileId => i
    }.getOrElse(-1))
    val animator = new DefaultItemAnimator()
    animator.setSupportsChangeAnimations(false) // prevent fading-in/out when rebinding
    profilesList.setItemAnimator(animator)
    profilesList.setAdapter(profilesAdapter)
    instance = this
    undoManager = new UndoSnackbarManager[Profile](getActivity.findViewById(R.id.snackbar),
      profilesAdapter.undo, profilesAdapter.commit)
    new ItemTouchHelper(new SimpleCallback(ItemTouchHelper.UP | ItemTouchHelper.DOWN,
      ItemTouchHelper.START | ItemTouchHelper.END) {
      override def getSwipeDirs(recyclerView: RecyclerView, viewHolder: ViewHolder): Int =
        if (isProfileEditable(viewHolder.asInstanceOf[ProfileViewHolder].item.id))
          super.getSwipeDirs(recyclerView, viewHolder) else 0
      override def getDragDirs(recyclerView: RecyclerView, viewHolder: ViewHolder): Int =
        if (isEnabled) super.getDragDirs(recyclerView, viewHolder) else 0

      def onSwiped(viewHolder: ViewHolder, direction: Int) {
        val index = viewHolder.getAdapterPosition
        profilesAdapter.remove(index)
        undoManager.remove((index, viewHolder.asInstanceOf[ProfileViewHolder].item))
      }
      def onMove(recyclerView: RecyclerView, viewHolder: ViewHolder, target: ViewHolder): Boolean = {
        profilesAdapter.move(viewHolder.getAdapterPosition, target.getAdapterPosition)
        true
      }
    }).attachToRecyclerView(profilesList)
  }

  override def onTrafficUpdated(profileId: Int, txRate: Long, rxRate: Long, txTotal: Long, rxTotal: Long): Unit =
    if (profileId != -1) {  // ignore resets from MainActivity
      if (bandwidthProfile != profileId) {
        onTrafficPersisted(bandwidthProfile)
        bandwidthProfile = profileId
      }
      this.txTotal = txTotal
      this.rxTotal = rxTotal
      profilesAdapter.refreshId(profileId)
    }
  def onTrafficPersisted(profileId: Int) {
    txTotal = 0
    rxTotal = 0
    if (bandwidthProfile != profileId) {
      onTrafficPersisted(bandwidthProfile)
      bandwidthProfile = profileId
    }
    profilesAdapter.deepRefreshId(profileId)
  }

  override def onDetach() {
    undoManager.flush()
    super.onDetach()
  }

  override def onDestroy() {
    instance = null
    super.onDestroy()
  }

  override def onActivityResult(requestCode: Int, resultCode: Int, data: Intent): Unit = requestCode match {
    case REQUEST_SCAN_QR_CODE => if (resultCode == Activity.RESULT_OK) {
      val contents = data.getStringExtra("SCAN_RESULT")
      if (!TextUtils.isEmpty(contents)) Parser.findAll(contents).foreach(app.profileManager.createProfile)
    }
    case _ => super.onActivityResult(resultCode, resultCode, data)
  }

  def onMenuItemClick(item: MenuItem): Boolean = item.getItemId match {
    case R.id.action_scan_qr_code =>
      try startActivityForResult(new Intent("com.google.zxing.client.android.SCAN")
        .addCategory(Intent.CATEGORY_DEFAULT)
        .addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_NEW_DOCUMENT),
        REQUEST_SCAN_QR_CODE) catch {
        case _: ActivityNotFoundException =>
          startActivity(new Intent(getActivity, classOf[ScannerActivity]))
        case e: SecurityException =>
          e.printStackTrace()
          app.track(e)
          startActivity(new Intent(getActivity, classOf[ScannerActivity]))
      }
      true
    case R.id.action_import =>
      try {
        val profiles = Parser.findAll(clipboard.getPrimaryClip.getItemAt(0).getText)
        if (profiles.nonEmpty) {
          profiles.foreach(app.profileManager.createProfile)
          Toast.makeText(getActivity, R.string.action_import_msg, Toast.LENGTH_SHORT).show()
          return true
        }
      } catch {
        case _: Exception =>
      }
      Snackbar.make(getActivity.findViewById(R.id.snackbar), R.string.action_import_err, Snackbar.LENGTH_LONG).show()
      true
    case R.id.action_manual_settings =>
      startConfig(app.profileManager.createProfile().id)
      true
    case R.id.action_export =>
      app.profileManager.getAllProfiles match {
        case Some(profiles) =>
          clipboard.setPrimaryClip(ClipData.newPlainText(null, profiles.mkString("\n")))
          Toast.makeText(getActivity, R.string.action_export_msg, Toast.LENGTH_SHORT).show()
        case _ => Toast.makeText(getActivity, R.string.action_export_err, Toast.LENGTH_SHORT).show()
      }
      true
    case _ => false
  }
}
