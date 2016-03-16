package com.github.shadowsocks

import java.nio.charset.Charset

import android.content._
import android.nfc.NfcAdapter.CreateNdefMessageCallback
import android.nfc.{NdefMessage, NdefRecord, NfcAdapter, NfcEvent}
import android.os.{Build, Bundle, Handler}
import android.provider.Settings
import android.support.v7.app.{AlertDialog, AppCompatActivity}
import android.support.v7.widget.RecyclerView.ViewHolder
import android.support.v7.widget.Toolbar.OnMenuItemClickListener
import android.support.v7.widget.helper.ItemTouchHelper
import android.support.v7.widget.helper.ItemTouchHelper.SimpleCallback
import android.support.v7.widget.{DefaultItemAnimator, LinearLayoutManager, RecyclerView, Toolbar}
import android.text.style.TextAppearanceSpan
import android.text.{SpannableStringBuilder, Spanned, TextUtils}
import android.view.{LayoutInflater, MenuItem, View, ViewGroup}
import android.widget.{CheckedTextView, ImageView, LinearLayout, Toast}
import com.github.shadowsocks.aidl.IShadowsocksServiceCallback
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.utils.{Key, Parser, TrafficMonitor, Utils}
import com.github.shadowsocks.widget.UndoSnackbarManager
import com.google.zxing.integration.android.IntentIntegrator
import net.glxn.qrgen.android.QRCode

import scala.collection.mutable.ArrayBuffer

class ProfileManagerActivity extends AppCompatActivity with OnMenuItemClickListener with ServiceBoundContext
  with CreateNdefMessageCallback {
  private class ProfileViewHolder(val view: View) extends RecyclerView.ViewHolder(view) with View.OnClickListener {
    var item: Profile = _
    private val text = itemView.findViewById(android.R.id.text1).asInstanceOf[CheckedTextView]
    itemView.setOnClickListener(this)

    {
      val shareBtn = itemView.findViewById(R.id.share)
      shareBtn.setOnClickListener(_ => {
        val url = item.toString
        if (nfcBeamEnable) {
          nfcAdapter.setNdefPushMessageCallback(ProfileManagerActivity.this,ProfileManagerActivity.this)
          nfcShareItem = url.getBytes(Charset.forName("UTF-8"))
        }
        val image = new ImageView(ProfileManagerActivity.this)
        image.setLayoutParams(new LinearLayout.LayoutParams(-1, -1))
        val qrcode = QRCode.from(url)
          .withSize(Utils.dpToPx(ProfileManagerActivity.this, 250), Utils.dpToPx(ProfileManagerActivity.this, 250))
          .asInstanceOf[QRCode].bitmap()
        image.setImageBitmap(qrcode)

        val dialog = new AlertDialog.Builder(ProfileManagerActivity.this)
          .setCancelable(true)
          .setPositiveButton(R.string.close, null)
          .setNegativeButton(R.string.copy_url, ((_, _) =>
            clipboard.setPrimaryClip(ClipData.newPlainText(null, url))): DialogInterface.OnClickListener)
          .setView(image)
          .setTitle(R.string.share)
          .create()
        if (!nfcAvailable){
          dialog.setMessage(getString(R.string.share_message_without_nfc))
        } else {
          if (!nfcBeamEnable) {
            dialog.setMessage(getString(R.string.share_message_nfc_disabled))
            dialog.setButton(DialogInterface.BUTTON_NEUTRAL, getString(R.string.turn_on_nfc), ((_, _) =>
              if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN) {
                startActivity(new Intent(Settings.ACTION_NFC_SETTINGS))
              } else {
                startActivity(new Intent(Settings.ACTION_WIRELESS_SETTINGS))
              }
              ): DialogInterface.OnClickListener)
          } else {
            dialog.setMessage(getString(R.string.share_message))
            dialog.setOnDismissListener(_ =>
              nfcAdapter.setNdefPushMessageCallback(null, ProfileManagerActivity.this))
          }
        }
        dialog.show()
      })
      shareBtn.setOnLongClickListener(_ => {
        Utils.positionToast(Toast.makeText(ProfileManagerActivity.this, R.string.share, Toast.LENGTH_SHORT), shareBtn,
          getWindow, 0, Utils.dpToPx(ProfileManagerActivity.this, 8)).show
        true
      })
    }

    def updateText(txTotal: Long = 0, rxTotal: Long = 0) {
      val builder = new SpannableStringBuilder
      val tx = item.tx + txTotal
      val rx = item.rx + rxTotal
      builder.append(item.name)
      if (tx != 0 || rx != 0) {
        val start = builder.length
        builder.append(getString(R.string.stat_profiles,
          TrafficMonitor.formatTraffic(tx), TrafficMonitor.formatTraffic(rx)))
        builder.setSpan(new TextAppearanceSpan(ProfileManagerActivity.this, android.R.style.TextAppearance_Small),
          start + 1, builder.length, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
      }
      handler.post(() => text.setText(builder))
    }

    def bind(item: Profile) {
      this.item = item
      updateText()
      if (item.id == ShadowsocksApplication.profileId) {
        text.setChecked(true)
        selectedItem = this
      } else {
        text.setChecked(false)
        if (selectedItem eq this) selectedItem = null
      }
    }

    def onClick(v: View) = {
      ShadowsocksApplication.switchProfile(item.id)
      finish
    }
  }

  private class ProfilesAdapter extends RecyclerView.Adapter[ProfileViewHolder] {
    var profiles = new ArrayBuffer[Profile]
    profiles ++= ShadowsocksApplication.profileManager.getAllProfiles.getOrElse(List.empty[Profile])

    def getItemCount = profiles.length

    def onBindViewHolder(vh: ProfileViewHolder, i: Int) = vh.bind(profiles(i))

    def onCreateViewHolder(vg: ViewGroup, i: Int) =
      new ProfileViewHolder(LayoutInflater.from(vg.getContext).inflate(R.layout.layout_profiles_item, vg, false))

    def add(item: Profile) {
      undoManager.flush
      val pos = getItemCount
      profiles += item
      notifyItemInserted(pos)
    }

    def move(from: Int, to: Int) {
      undoManager.flush
      val step = if (from < to) 1 else -1
      val first = profiles(from)
      var previousOrder = profiles(from).userOrder
      for (i <- from until to by step) {
        val next = profiles(i + step)
        val order = next.userOrder
        next.userOrder = previousOrder
        previousOrder = order
        profiles(i) = next
        ShadowsocksApplication.profileManager.updateProfile(next)
      }
      first.userOrder = previousOrder
      profiles(to) = first
      ShadowsocksApplication.profileManager.updateProfile(first)
      notifyItemMoved(from, to)
    }

    def remove(pos: Int) {
      profiles.remove(pos)
      notifyItemRemoved(pos)
    }
    def undo(actions: Iterator[(Int, Profile)]) = for ((index, item) <- actions) {
      profiles.insert(index, item)
      notifyItemInserted(index)
    }
    def commit(actions: Iterator[(Int, Profile)]) = for ((index, item) <- actions) {
      ShadowsocksApplication.profileManager.delProfile(item.id)
      if (item.id == ShadowsocksApplication.profileId) ShadowsocksApplication.profileId(-1)
    }
  }

  private var selectedItem: ProfileViewHolder = _
  private val handler = new Handler

  private lazy val profilesAdapter = new ProfilesAdapter
  private var undoManager: UndoSnackbarManager[Profile] = _

  private lazy val clipboard = getSystemService(Context.CLIPBOARD_SERVICE).asInstanceOf[ClipboardManager]

  private var nfcAdapter : NfcAdapter = _
  private var nfcShareItem: Array[Byte] = _
  private var nfcAvailable = false
  private var nfcEnable = false
  private var nfcBeamEnable = false

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
    toolbar.inflateMenu(R.menu.profile_manager_menu)
    toolbar.setOnMenuItemClickListener(this)

    ShadowsocksApplication.profileManager.setProfileAddedListener(profilesAdapter.add)
    val profilesList = findViewById(R.id.profilesList).asInstanceOf[RecyclerView]
    val layoutManager = new LinearLayoutManager(this)
    profilesList.setLayoutManager(layoutManager)
    profilesList.setItemAnimator(new DefaultItemAnimator)
    profilesList.setAdapter(profilesAdapter)
    layoutManager.scrollToPosition(profilesAdapter.profiles.zipWithIndex.collectFirst {
      case (profile, i) if profile.id == ShadowsocksApplication.profileId => i
    }.getOrElse(-1))
    undoManager = new UndoSnackbarManager[Profile](profilesList, profilesAdapter.undo, profilesAdapter.commit)
    new ItemTouchHelper(new SimpleCallback(ItemTouchHelper.UP | ItemTouchHelper.DOWN,
      ItemTouchHelper.START | ItemTouchHelper.END) {
      def onSwiped(viewHolder: ViewHolder, direction: Int) = {
        val index = viewHolder.getAdapterPosition
        profilesAdapter.remove(index)
        undoManager.remove(index, viewHolder.asInstanceOf[ProfileViewHolder].item)
      }
      def onMove(recyclerView: RecyclerView, viewHolder: ViewHolder, target: ViewHolder) = {
        profilesAdapter.move(viewHolder.getAdapterPosition, target.getAdapterPosition)
        true
      }
    }).attachToRecyclerView(profilesList)

    attachService(new IShadowsocksServiceCallback.Stub {
      def stateChanged(state: Int, msg: String) = () // ignore
      def trafficUpdated(txRate: Long, rxRate: Long, txTotal: Long, rxTotal: Long) =
        if (selectedItem != null) selectedItem.updateText(txTotal, rxTotal)
    })

    if (ShadowsocksApplication.settings.getBoolean(Key.profileTip, true)) {
      ShadowsocksApplication.settings.edit.putBoolean(Key.profileTip, false).commit
      new AlertDialog.Builder(this).setTitle(R.string.profile_manager_dialog)
        .setMessage(R.string.profile_manager_dialog_content).setPositiveButton(R.string.gotcha, null).create.show
    }
  }


  override def onResume() {
    super.onResume()
    nfcAdapter = NfcAdapter.getDefaultAdapter(this)
    if (nfcAdapter != null){
      nfcAvailable = true
      if (nfcAdapter.isEnabled) {
        nfcEnable = true
        if (nfcAdapter.isNdefPushEnabled) {
          nfcBeamEnable = true
          nfcAdapter.setNdefPushMessageCallback(null, ProfileManagerActivity.this)
        } else {
          nfcBeamEnable = false
        }
      } else{
        nfcEnable = false
        nfcBeamEnable = false
      }
    } else {
      nfcAvailable = false
      nfcEnable = false
      nfcBeamEnable = false
    }
  }

  override def onStart() {
    super.onStart()
    registerCallback
  }
  override def onStop() {
    super.onStop()
    unregisterCallback
  }

  override def onDestroy {
    deattachService()
    undoManager.flush
    ShadowsocksApplication.profileManager.setProfileAddedListener(null)
    super.onDestroy
  }

  override def onActivityResult(requestCode: Int, resultCode: Int, data: Intent) {
    val scanResult = IntentIntegrator.parseActivityResult(requestCode, resultCode, data)
    if (scanResult != null) {
      val contents = scanResult.getContents
      if (!TextUtils.isEmpty(contents))
        Parser.findAll(contents).foreach(ShadowsocksApplication.profileManager.createProfile)
    }
  }

  def createNdefMessage(nfcEvent: NfcEvent) =
    new NdefMessage(Array(new NdefRecord(NdefRecord.TNF_ABSOLUTE_URI, nfcShareItem, Array[Byte](), nfcShareItem)))

  def onMenuItemClick(item: MenuItem): Boolean = item.getItemId match {
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
    case R.id.action_import =>
      if (clipboard.hasPrimaryClip) {
        val profiles = Parser.findAll(clipboard.getPrimaryClip.getItemAt(0).getText)
        if (profiles.nonEmpty) {
          profiles.foreach(ShadowsocksApplication.profileManager.createProfile)
          Toast.makeText(this, R.string.action_import_msg, Toast.LENGTH_SHORT).show
          return true
        }
      }
      Toast.makeText(this, R.string.action_import_err, Toast.LENGTH_SHORT).show
      true
    case R.id.action_export =>
      ShadowsocksApplication.profileManager.getAllProfiles match {
        case Some(profiles) =>
          clipboard.setPrimaryClip(ClipData.newPlainText(null, profiles.mkString("\n")))
          Toast.makeText(this, R.string.action_export_msg, Toast.LENGTH_SHORT).show
        case _ => Toast.makeText(this, R.string.action_export_err, Toast.LENGTH_SHORT).show
      }
      true
    case _ => false
  }
}
