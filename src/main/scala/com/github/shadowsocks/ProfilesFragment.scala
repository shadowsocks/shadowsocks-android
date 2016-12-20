/*******************************************************************************/
/*                                                                             */
/*  Copyright (C) 2016 by Max Lv <max.c.lv@gmail.com>                          */
/*  Copyright (C) 2016 by Mygod Studio <contact-shadowsocks-android@mygod.be>  */
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

import android.content._
import android.graphics.Typeface
import android.os.{Build, Bundle, Handler, UserManager}
import android.support.v4.content.LocalBroadcastManager
import android.support.v7.widget.RecyclerView.ViewHolder
import android.support.v7.widget.Toolbar.OnMenuItemClickListener
import android.support.v7.widget._
import android.support.v7.widget.helper.ItemTouchHelper
import android.support.v7.widget.helper.ItemTouchHelper.SimpleCallback
import android.view._
import android.widget.{TextView, Toast}
import com.github.shadowsocks.ShadowsocksApplication.app
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.utils._
import com.github.shadowsocks.widget.UndoSnackbarManager

import scala.collection.mutable.ArrayBuffer

final class ProfilesFragment extends ToolbarFragment with OnMenuItemClickListener {
  private final class ProfileViewHolder(val view: View) extends RecyclerView.ViewHolder(view)
    with View.OnClickListener {

    var item: Profile = _
    private val text1 = itemView.findViewById(R.id.title).asInstanceOf[TextView]
    private val text2 = itemView.findViewById(R.id.address).asInstanceOf[TextView]
    private val text3 = itemView.findViewById(R.id.traffic).asInstanceOf[TextView]
    private val indicator = itemView.findViewById(R.id.indicator).asInstanceOf[ViewGroup]
    itemView.setOnClickListener(this)

    {
      val edit = itemView.findViewById(R.id.edit)
      edit.setOnClickListener(_ => startActivity(new Intent(getActivity, classOf[ProfileConfigActivity])
        .putExtra(Action.EXTRA_PROFILE_ID, item.id)))
      edit.setOnLongClickListener(_ => {
        Utils.positionToast(Toast.makeText(getActivity, edit.getContentDescription, Toast.LENGTH_SHORT), edit,
          getActivity.getWindow, 0, Utils.dpToPx(getActivity, 8)).show()
        true
      })
    }

    def updateText(txTotal: Long = 0, rxTotal: Long = 0) {
      val tx = item.tx + txTotal
      val rx = item.rx + rxTotal
      var title = if (isDemoMode) "Profile #" + item.id else item.name
      val address = (if (item.host.contains(":")) "[%s]:%d" else "%s:%d").format(item.host, item.remotePort)
      if (title == null || title.isEmpty) title = address
      val traffic = getString(R.string.stat_profiles,
        TrafficMonitor.formatTraffic(tx), TrafficMonitor.formatTraffic(rx))

      handler.post(() => {
        text1.setText(title)
        text2.setText(address)
        text3.setText(traffic)
      })
    }

    def bind(item: Profile) {
      this.item = item
      updateText()
      if (item.id == app.profileId) {
        // text1.setTypeface(null, Typeface.BOLD)
        indicator.setBackgroundResource(R.drawable.background_selected)
        selectedItem = this
      } else {
        // text1.setTypeface(null, Typeface.NORMAL)
        indicator.setBackgroundResource(R.drawable.background_selectable)
        if (selectedItem eq this) selectedItem = null
      }
    }

    def onClick(v: View) {
      val activity = getActivity.asInstanceOf[MainActivity]
      val state = activity.state
      if (state == State.STOPPED || state == State.CONNECTED) {
        val old = app.profileId
        app.switchProfile(item.id)
        profilesAdapter.refreshId(old)
        bind(item)
        if (state == State.CONNECTED) activity.serviceLoad()
      }
    }
  }

  private class ProfilesAdapter extends RecyclerView.Adapter[ProfileViewHolder] {
    var profiles = new ArrayBuffer[Profile]
    profiles ++= app.profileManager.getAllProfiles.getOrElse(List.empty[Profile])

    def getItemCount: Int = profiles.length

    def onBindViewHolder(vh: ProfileViewHolder, i: Int): Unit = vh.bind(profiles(i))

    def onCreateViewHolder(vg: ViewGroup, i: Int) =
      new ProfileViewHolder(LayoutInflater.from(vg.getContext).inflate(R.layout.layout_profiles_item, vg, false))

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
    def commit(actions: Iterator[(Int, Profile)]): Unit = for ((_, item) <- actions) {
      app.profileManager.delProfile(item.id)
      if (item.id == app.profileId) app.profileId(-1)
    }

    def refreshId(id: Int) {
      val index = profiles.indexWhere(_.id == id)
      profiles(index) = app.profileManager.getProfile(id).get
      notifyItemChanged(index)
    }
    def removeId(id: Int) {
      val index = profiles.indexWhere(_.id == id)
      profiles.remove(index)
      notifyItemRemoved(index)
    }
  }

  private var selectedItem: ProfileViewHolder = _
  private val handler = new Handler
  private val profilesListener: BroadcastReceiver = (_, intent) => {
    val id = intent.getIntExtra(Action.EXTRA_PROFILE_ID, -1)
    intent.getAction match {
      case Action.PROFILE_CHANGED => profilesAdapter.refreshId(id)
      case Action.PROFILE_REMOVED => profilesAdapter.removeId(id)
    }
  }

  private lazy val profilesAdapter = new ProfilesAdapter
  private var undoManager: UndoSnackbarManager[Profile] = _

  private lazy val clipboard = getActivity.getSystemService(Context.CLIPBOARD_SERVICE).asInstanceOf[ClipboardManager]
  private lazy val isDemoMode = Build.VERSION.SDK_INT >= 25 &&
    getActivity.getSystemService(classOf[UserManager]).isDemoUser

  override def onCreateView(inflater: LayoutInflater, container: ViewGroup, savedInstanceState: Bundle): View =
    inflater.inflate(R.layout.layout_profiles, container, false)

  override def onViewCreated(view: View, savedInstanceState: Bundle) {
    super.onViewCreated(view, savedInstanceState)
    toolbar.setTitle(R.string.profiles)
    toolbar.inflateMenu(R.menu.profile_manager_menu)
    toolbar.setOnMenuItemClickListener(this)

    app.profileManager.setProfileAddedListener(profilesAdapter.add)
    val profilesList = view.findViewById(R.id.profilesList).asInstanceOf[RecyclerView]
    val layoutManager = new LinearLayoutManager(getActivity)
    profilesList.setLayoutManager(layoutManager)
    profilesList.setItemAnimator(new DefaultItemAnimator)
    profilesList.setAdapter(profilesAdapter)
    layoutManager.scrollToPosition(profilesAdapter.profiles.zipWithIndex.collectFirst {
      case (profile, i) if profile.id == app.profileId => i
    }.getOrElse(-1))
    undoManager = new UndoSnackbarManager[Profile](profilesList, profilesAdapter.undo, profilesAdapter.commit)
    new ItemTouchHelper(new SimpleCallback(ItemTouchHelper.UP | ItemTouchHelper.DOWN,
      ItemTouchHelper.START | ItemTouchHelper.END) {
      def onSwiped(viewHolder: ViewHolder, direction: Int) {
        val index = viewHolder.getAdapterPosition
        profilesAdapter.remove(index)
        undoManager.remove(index, viewHolder.asInstanceOf[ProfileViewHolder].item)
      }
      def onMove(recyclerView: RecyclerView, viewHolder: ViewHolder, target: ViewHolder): Boolean = {
        profilesAdapter.move(viewHolder.getAdapterPosition, target.getAdapterPosition)
        true
      }
    }).attachToRecyclerView(profilesList)
    val filter = new IntentFilter(Action.PROFILE_CHANGED)
    filter.addAction(Action.PROFILE_REMOVED)
    LocalBroadcastManager.getInstance(getActivity).registerReceiver(profilesListener, filter)
  }

  override def onTrafficUpdated(txRate: Long, rxRate: Long, txTotal: Long, rxTotal: Long): Unit =
    if (selectedItem != null) selectedItem.updateText(txTotal, rxTotal)

  override def onDetach() {
    undoManager.flush()
    super.onDetach()
  }

  override def onDestroy() {
    LocalBroadcastManager.getInstance(getActivity).unregisterReceiver(profilesListener)
    super.onDestroy()
  }

  def onMenuItemClick(item: MenuItem): Boolean = item.getItemId match {
    case R.id.action_scan_qr_code =>
      startActivity(new Intent(getActivity, classOf[ScannerActivity]))
      true
    case R.id.action_import =>
      if (clipboard.hasPrimaryClip) {
        val profiles = Parser.findAll(clipboard.getPrimaryClip.getItemAt(0).getText)
        if (profiles.nonEmpty) {
          profiles.foreach(app.profileManager.createProfile)
          Toast.makeText(getActivity, R.string.action_import_msg, Toast.LENGTH_SHORT).show()
          return true
        }
      }
      Toast.makeText(getActivity, R.string.action_import_err, Toast.LENGTH_SHORT).show()
      true
    case R.id.action_manual_settings =>
      val profile = app.profileManager.createProfile()
      app.profileManager.updateProfile(profile)
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
