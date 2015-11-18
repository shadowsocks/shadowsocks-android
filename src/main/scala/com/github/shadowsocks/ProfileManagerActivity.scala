package com.github.shadowsocks

import android.app.AlertDialog
import android.content.{DialogInterface, Intent}
import android.os.Bundle
import android.support.design.widget.Snackbar
import android.support.v7.app.AppCompatActivity
import android.support.v7.widget.RecyclerView.ViewHolder
import android.support.v7.widget.Toolbar.OnMenuItemClickListener
import android.support.v7.widget.helper.ItemTouchHelper
import android.support.v7.widget.helper.ItemTouchHelper.SimpleCallback
import android.support.v7.widget.{DefaultItemAnimator, LinearLayoutManager, RecyclerView, Toolbar}
import android.view.View.{OnAttachStateChangeListener, OnClickListener}
import android.view.{LayoutInflater, MenuItem, View, ViewGroup}
import android.widget.{LinearLayout, ImageView, CheckedTextView, Toast}
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.utils.{Parser, Utils}
import com.google.zxing.integration.android.IntentIntegrator
import net.glxn.qrgen.android.QRCode

import scala.collection.mutable.ArrayBuffer

/**
  * @author Mygod
  */
class ProfileManagerActivity extends AppCompatActivity with OnMenuItemClickListener {
  private class ProfileViewHolder(val view: View) extends RecyclerView.ViewHolder(view) with View.OnClickListener {
    private var item: Profile = _
    private val text = itemView.findViewById(android.R.id.text1).asInstanceOf[CheckedTextView]
    itemView.setOnClickListener(this)

    {
      val qrcode = itemView.findViewById(R.id.qrcode)
      qrcode.setOnClickListener((v: View) => {
        val image = new ImageView(ProfileManagerActivity.this)
        image.setLayoutParams(new LinearLayout.LayoutParams(-1, -1))
        val qrcode = QRCode.from(Parser.generate(item))
          .withSize(Utils.dpToPx(ProfileManagerActivity.this, 250), Utils.dpToPx(ProfileManagerActivity.this, 250))
          .asInstanceOf[QRCode]
        image.setImageBitmap(qrcode.bitmap())

        new AlertDialog.Builder(ProfileManagerActivity.this)
          .setCancelable(true)
          .setNegativeButton(getString(R.string.close),
            ((dialog: DialogInterface, id: Int) => dialog.cancel()): DialogInterface.OnClickListener)
          .setView(image)
          .create()
          .show()
      })
      qrcode.setOnLongClickListener((v: View) => {
        Utils.positionToast(Toast.makeText(ProfileManagerActivity.this, R.string.qrcode, Toast.LENGTH_SHORT), qrcode,
          getWindow, 0, Utils.dpToPx(ProfileManagerActivity.this, 8)).show
        true
      })
    }

    def bind(item: Profile) {
      text.setText(item.name)
      text.setChecked(item.id == ShadowsocksApplication.profileId)
      this.item = item
    }

    def onClick(v: View) = {
      ShadowsocksApplication.switchProfile(item.id)
      finish
    }
  }

  private class ProfilesAdapter extends RecyclerView.Adapter[ProfileViewHolder] {
    private val recycleBin = new ArrayBuffer[(Int, Profile)]
    private var profiles = new ArrayBuffer[Profile]
    profiles ++= ShadowsocksApplication.profileManager.getAllProfiles.getOrElse(List[Profile]())

    def getItemCount = profiles.length

    def onBindViewHolder(vh: ProfileViewHolder, i: Int) = vh.bind(profiles(i))

    def onCreateViewHolder(vg: ViewGroup, i: Int) =
      new ProfileViewHolder(LayoutInflater.from(vg.getContext).inflate(R.layout.layout_profiles_item, vg, false))

    def add(item: Profile) {
      removedSnackbar.dismiss
      commitRemoves
      val pos = getItemCount
      profiles += item
      notifyItemInserted(pos)
    }

    def remove(pos: Int) {
      recycleBin.append((pos, profiles(pos)))
      profiles.remove(pos)
      notifyItemRemoved(pos)
    }
    def undoRemoves {
      for ((index, item) <- recycleBin.reverseIterator) {
        profiles.insert(index, item)
        notifyItemInserted(index)
      }
      recycleBin.clear
    }
    def commitRemoves {
      for ((index, item) <- recycleBin) {
        ShadowsocksApplication.profileManager.delProfile(item.id)
        if (item.id == ShadowsocksApplication.profileId) ShadowsocksApplication.profileId(-1)
      }
      recycleBin.clear
    }
  }

  private lazy val profilesAdapter = new ProfilesAdapter
  private var removedSnackbar: Snackbar = _

  override def onCreate(savedInstanceState: Bundle) {
    super.onCreate(savedInstanceState)
    setContentView(R.layout.layout_profiles)

    val toolbar = findViewById(R.id.toolbar).asInstanceOf[Toolbar]
    toolbar.setTitle(R.string.profiles)
    toolbar.setNavigationIcon(R.drawable.abc_ic_ab_back_mtrl_am_alpha)
    toolbar.setNavigationOnClickListener((v: View) => {
      val intent = getParentActivityIntent
      if (intent == null) finish else navigateUpTo(intent)
    })
    toolbar.inflateMenu(R.menu.add_profile_methods)
    toolbar.setOnMenuItemClickListener(this)

    ShadowsocksApplication.profileManager.setProfileAddedListener(profilesAdapter.add)
    val profilesList = findViewById(R.id.profilesList).asInstanceOf[RecyclerView]
    profilesList.setLayoutManager(new LinearLayoutManager(this))
    profilesList.setItemAnimator(new DefaultItemAnimator)
    profilesList.setAdapter(profilesAdapter)
    removedSnackbar = Snackbar.make(findViewById(android.R.id.content), R.string.removed, Snackbar.LENGTH_LONG)
      .setAction(R.string.undo, ((v: View) => profilesAdapter.undoRemoves): OnClickListener)
    removedSnackbar.getView.addOnAttachStateChangeListener(new OnAttachStateChangeListener {
      def onViewDetachedFromWindow(v: View) = profilesAdapter.commitRemoves
      def onViewAttachedToWindow(v: View) = ()
    })
    new ItemTouchHelper(new SimpleCallback(0, ItemTouchHelper.START | ItemTouchHelper.END) {
      def onSwiped(viewHolder: ViewHolder, direction: Int) = {
        profilesAdapter.remove(viewHolder.getAdapterPosition)
        removedSnackbar.show
      }
      def onMove(recyclerView: RecyclerView, viewHolder: ViewHolder, target: ViewHolder) = false  // TODO?
    }).attachToRecyclerView(profilesList)
  }

  override def onDestroy {
    super.onDestroy
    ShadowsocksApplication.profileManager.setProfileAddedListener(null)
    profilesAdapter.commitRemoves
  }

  override def onActivityResult(requestCode: Int, resultCode: Int, data: Intent) {
    val scanResult = IntentIntegrator.parseActivityResult(requestCode, resultCode, data)
    if (scanResult != null) Parser.parse(scanResult.getContents) match {
      case Some(profile) => ShadowsocksApplication.profileManager.createProfile(profile)
      case _ => // ignore
    }
  }

  def onMenuItemClick(item: MenuItem) = item.getItemId match {
    case R.id.scan_qr_code =>
      val integrator = new IntentIntegrator(this)
      val list = new java.util.ArrayList(IntentIntegrator.TARGET_ALL_KNOWN)
      list.add("tw.com.quickmark")
      integrator.setTargetApplications(list)
      integrator.initiateScan()
      true
    case R.id.manual_settings =>
      ShadowsocksApplication.profileManager.reload(-1)
      ShadowsocksApplication.switchProfile(ShadowsocksApplication.profileManager.save.id)
      finish
      true
    case _ => false
  }
}
