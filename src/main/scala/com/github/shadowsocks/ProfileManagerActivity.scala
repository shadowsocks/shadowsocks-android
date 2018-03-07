package com.github.shadowsocks

import java.nio.charset.Charset
import java.util.Locale
import java.io.File

import java.net._

import android.app.{Activity, TaskStackBuilder, ProgressDialog}
import android.content._
import android.content.pm.PackageManager
import android.nfc.NfcAdapter.CreateNdefMessageCallback
import android.nfc.{NdefMessage, NdefRecord, NfcAdapter, NfcEvent}
import android.os._
import android.provider.Settings
import android.support.v7.app.{AlertDialog, AppCompatActivity}
import android.support.v7.widget.RecyclerView.ViewHolder
import android.support.v7.widget.Toolbar.OnMenuItemClickListener
import android.support.v7.widget._
import android.support.v7.widget.helper.ItemTouchHelper
import android.support.v7.widget.helper.ItemTouchHelper.SimpleCallback
import android.text.style.TextAppearanceSpan
import android.text.{SpannableStringBuilder, Spanned, TextUtils}
import android.view._
import android.widget.{CheckedTextView, ImageView, LinearLayout, Toast, Switch, CompoundButton, TextView, EditText}
import android.net.Uri
import java.io.IOException
import android.support.design.widget.Snackbar
import com.github.clans.fab.{FloatingActionButton, FloatingActionMenu}
import com.github.shadowsocks.ShadowsocksApplication.app
import com.github.shadowsocks.aidl.IShadowsocksServiceCallback
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.database.SSRSub
import com.github.shadowsocks.utils.{Key, Parser, TrafficMonitor, Utils}
import com.github.shadowsocks.widget.UndoSnackbarManager
import com.github.shadowsocks.utils._
import com.github.shadowsocks.utils.CloseUtils._
import net.glxn.qrgen.android.QRCode
import java.lang.System.currentTimeMillis
import java.lang.Thread
import java.util.Random
import android.util.{Base64, Log}
import android.content.DialogInterface._
import okhttp3._
import java.util.concurrent.TimeUnit
import android.preference.PreferenceManager
import scala.collection.mutable.ArrayBuffer

final class ProfileManagerActivity extends AppCompatActivity with OnMenuItemClickListener with ServiceBoundContext
  with View.OnClickListener with CreateNdefMessageCallback {

  private final class ProfileViewHolder(val view: View) extends RecyclerView.ViewHolder(view)
    with View.OnClickListener with View.OnKeyListener {

    var item: Profile = _
    private val text = itemView.findViewById(android.R.id.text1).asInstanceOf[CheckedTextView]
    itemView.setOnClickListener(this)
    itemView.setOnKeyListener(this)

    {
      val shareBtn = itemView.findViewById(R.id.share)
      shareBtn.setOnClickListener(_ => {
        val url = item.toString
        if (isNfcBeamEnabled) {
          nfcAdapter.setNdefPushMessageCallback(ProfileManagerActivity.this,ProfileManagerActivity.this)
          nfcShareItem = url.getBytes(Charset.forName("UTF-8"))
        }
        val image = new ImageView(ProfileManagerActivity.this)
        image.setLayoutParams(new LinearLayout.LayoutParams(-1, -1))
        val qrcode = QRCode.from(url)
          .withSize(Utils.dpToPx(ProfileManagerActivity.this, 250), Utils.dpToPx(ProfileManagerActivity.this, 250))
          .asInstanceOf[QRCode].bitmap()
        image.setImageBitmap(qrcode)

        val dialog = new AlertDialog.Builder(ProfileManagerActivity.this, R.style.Theme_Material_Dialog_Alert)
          .setCancelable(true)
          .setPositiveButton(R.string.close, null)
          .setNegativeButton(R.string.copy_url, ((_, _) =>
            clipboard.setPrimaryClip(ClipData.newPlainText(null, url))): DialogInterface.OnClickListener)
          .setView(image)
          .setTitle(R.string.share)
          .create()
        if (!isNfcAvailable) dialog.setMessage(getString(R.string.share_message_without_nfc))
        else if (!isNfcBeamEnabled) {
          dialog.setMessage(getString(R.string.share_message_nfc_disabled))
          dialog.setButton(DialogInterface.BUTTON_NEUTRAL, getString(R.string.turn_on_nfc),
            ((_, _) => startActivity(new Intent(Settings.ACTION_NFC_SETTINGS))): DialogInterface.OnClickListener)
        } else {
          dialog.setMessage(getString(R.string.share_message))
          dialog.setOnDismissListener(_ =>
            nfcAdapter.setNdefPushMessageCallback(null, ProfileManagerActivity.this))
        }
        dialog.show()
      })
      shareBtn.setOnLongClickListener(_ => {
        Utils.positionToast(Toast.makeText(ProfileManagerActivity.this, R.string.share, Toast.LENGTH_SHORT), shareBtn,
          getWindow, 0, Utils.dpToPx(ProfileManagerActivity.this, 8)).show
        true
      })
    }

    {
      val pingBtn = itemView.findViewById(R.id.ping_single)
      pingBtn.setOnClickListener(_ => {

        val singleTestProgressDialog = ProgressDialog.show(ProfileManagerActivity.this, getString(R.string.tips_testing), getString(R.string.tips_testing), false, true)

        var profile = item

        Utils.ThrowableFuture {

          // Resolve the server address
          var host = profile.host;
          if (!Utils.isNumeric(host)) Utils.resolve(host, enableIPv6 = true) match {
            case Some(addr) => host = addr
            case None => throw new Exception("can't resolve")
          }

          val conf = ConfigUtils
            .SHADOWSOCKS.formatLocal(Locale.ENGLISH, host, profile.remotePort, profile.localPort + 2,
              ConfigUtils.EscapedJson(profile.password), profile.method, 600, profile.protocol, profile.obfs, ConfigUtils.EscapedJson(profile.obfs_param), ConfigUtils.EscapedJson(profile.protocol_param))
          Utils.printToFile(new File(getApplicationInfo.dataDir + "/ss-local-test.conf"))(p => {
            p.println(conf)
          })

          val cmd = ArrayBuffer[String](getApplicationInfo.dataDir + "/ss-local"
            , "-t", "600"
            , "-L", "www.google.com:80"
            , "-c", getApplicationInfo.dataDir + "/ss-local-test.conf")

          if (TcpFastOpen.sendEnabled) cmd += "--fast-open"

          if (ssTestProcess != null) {
            ssTestProcess.destroy()
            ssTestProcess = null
          }

          ssTestProcess = new GuardedProcess(cmd).start()

          val start = currentTimeMillis
          while (start - currentTimeMillis < 5 * 1000 && isPortAvailable(profile.localPort + 2)) {
            try {
              Thread.sleep(50)
            } catch{
              case e: InterruptedException => Unit
            }
          }
          //val proxy = new Proxy(Proxy.Type.SOCKS, new InetSocketAddress("127.0.0.1", profile.localPort + 2))

          // Based on: https://android.googlesource.com/platform/frameworks/base/+/master/services/core/java/com/android/server/connectivity/NetworkMonitor.java#640

          //okhttp
          var result = ""
          val builder = new OkHttpClient.Builder()
                          .connectTimeout(5, TimeUnit.SECONDS)
                          .writeTimeout(5, TimeUnit.SECONDS)
                          .readTimeout(5, TimeUnit.SECONDS)

          val client = builder.build();

          val request = new Request.Builder()
            .url("http://127.0.0.1:" + (profile.localPort + 2) + "/generate_204").removeHeader("Host").addHeader("Host", "www.google.com")
            .build();

          try {
            val response = client.newCall(request).execute()
            val code = response.code()
            if (code == 204 || code == 200 && response.body().contentLength == 0) {
              val start = currentTimeMillis
              val response = client.newCall(request).execute()
              val elapsed = currentTimeMillis - start
              val code = response.code()
              if (code == 204 || code == 200 && response.body().contentLength == 0)
              {
                result = getString(R.string.connection_test_available, elapsed: java.lang.Long)
                profile.elapsed = elapsed
                app.profileManager.updateProfile(profile)

                this.updateText(0, 0, elapsed)
              }
              else throw new Exception(getString(R.string.connection_test_error_status_code, code: Integer))
              response.body().close()
            } else throw new Exception(getString(R.string.connection_test_error_status_code, code: Integer))
            response.body().close()
          } catch {
            case e: IOException =>
              result = getString(R.string.connection_test_error, e.getMessage)
          }

          Snackbar.make(findViewById(android.R.id.content), result, Snackbar.LENGTH_LONG).show

          /*autoDisconnect(new URL("https", "www.google.com", "/generate_204").openConnection(proxy)
            .asInstanceOf[HttpURLConnection]) { conn =>
            conn.setConnectTimeout(5 * 1000)
            conn.setReadTimeout(5 * 1000)
            conn.setInstanceFollowRedirects(false)
            conn.setUseCaches(false)
            var result: String = null
            var success = true
            try {
              conn.getInputStream
              val code = conn.getResponseCode
              if (code == 204 || code == 200 && conn.getContentLength == 0)
              {
                autoDisconnect(new URL("https", "www.google.com", "/generate_204").openConnection(proxy)
                  .asInstanceOf[HttpURLConnection]) { conn =>
                  conn.setConnectTimeout(5 * 1000)
                  conn.setReadTimeout(5 * 1000)
                  conn.setInstanceFollowRedirects(false)
                  conn.setUseCaches(false)
                  var result: String = null
                  var success = true
                  try {
                    val start = currentTimeMillis
                    conn.getInputStream
                    val elapsed = currentTimeMillis - start
                    val code = conn.getResponseCode
                    if (code == 204 || code == 200 && conn.getContentLength == 0)
                    {
                      result = getString(R.string.connection_test_available, elapsed: java.lang.Long)
                      profile.elapsed = elapsed
                      app.profileManager.updateProfile(profile)

                      this.updateText(0, 0, elapsed)
                    }
                    else throw new Exception(getString(R.string.connection_test_error_status_code, code: Integer))
                  } catch {
                    case e: Exception =>
                      success = false
                      result = getString(R.string.connection_test_error, e.getMessage)
                  }
                  Snackbar.make(findViewById(android.R.id.content), result, Snackbar.LENGTH_LONG).show
                }
              }
              else throw new Exception(getString(R.string.connection_test_error_status_code, code: Integer))
            } catch {
              case e: Exception =>
                success = false
                result = getString(R.string.connection_test_error, e.getMessage)
                Snackbar.make(findViewById(android.R.id.content), result, Snackbar.LENGTH_LONG).show
            }
          }*/

          if (ssTestProcess != null) {
            ssTestProcess.destroy()
            ssTestProcess = null
          }

          singleTestProgressDialog.dismiss()
        }

        // Based on: https://android.googlesource.com/platform/frameworks/base/+/master/services/core/java/com/android/server/connectivity/NetworkMonitor.java#640
      })
      pingBtn.setOnLongClickListener(_ => {
        Utils.positionToast(Toast.makeText(ProfileManagerActivity.this, R.string.ping, Toast.LENGTH_SHORT), pingBtn,
          getWindow, 0, Utils.dpToPx(ProfileManagerActivity.this, 8)).show
        true
      })
    }

    def updateText(txTotal: Long = 0, rxTotal: Long = 0, elapsedInput: Long = -1) {
      val builder = new SpannableStringBuilder
      val tx = item.tx + txTotal
      val rx = item.rx + rxTotal
      var elapsed = item.elapsed
      if (elapsedInput != -1) {
        elapsed = elapsedInput
      }
      builder.append(item.name)
      if (tx != 0 || rx != 0 || elapsed != 0 || item.url_group != "") {
        val start = builder.length
        builder.append(getString(R.string.stat_profiles,
          TrafficMonitor.formatTraffic(tx), TrafficMonitor.formatTraffic(rx), String.valueOf(elapsed), item.url_group))
        builder.setSpan(new TextAppearanceSpan(ProfileManagerActivity.this, android.R.style.TextAppearance_Small),
          start + 1, builder.length, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
      }
      handler.post(() => text.setText(builder))
    }

    def bind(item: Profile) {
      this.item = item
      updateText()
      if (item.id == app.profileId) {
        text.setChecked(true)
        selectedItem = this
      } else {
        text.setChecked(false)
        if (selectedItem eq this) selectedItem = null
      }
    }

    def onClick(v: View) {
      app.switchProfile(item.id)
      finish
    }

    def onKey(v: View, keyCode: Int, event: KeyEvent) = if (event.getAction == KeyEvent.ACTION_DOWN) keyCode match {
      case KeyEvent.KEYCODE_DPAD_LEFT =>
        val index = getAdapterPosition
        if (index >= 0) {
          profilesAdapter.remove(index)
          undoManager.remove(index, item)
          true
        } else false
      case _ => false
    } else false
  }

  private class ProfilesAdapter extends RecyclerView.Adapter[ProfileViewHolder] {
    var profiles = new ArrayBuffer[Profile]
    if (is_sort) {
      profiles ++= app.profileManager.getAllProfilesByElapsed.getOrElse(List.empty[Profile])
    } else {
      profiles ++= app.profileManager.getAllProfiles.getOrElse(List.empty[Profile])
    }

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
    def undo(actions: Iterator[(Int, Profile)]) = for ((index, item) <- actions) {
      profiles.insert(index, item)
      notifyItemInserted(index)
    }
    def commit(actions: Iterator[(Int, Profile)]) = for ((index, item) <- actions) {
      app.profileManager.delProfile(item.id)
      if (item.id == app.profileId) app.profileId(-1)
    }
  }

  private final class SSRSubViewHolder(val view: View) extends RecyclerView.ViewHolder(view)
    with View.OnClickListener with View.OnKeyListener {

    var item: SSRSub = _
    private val text = itemView.findViewById(android.R.id.text2).asInstanceOf[TextView]
    itemView.setOnClickListener(this)

    def updateText(isShowUrl: Boolean = false) {
      val builder = new SpannableStringBuilder
      builder.append(this.item.url_group + "\n")
      if (isShowUrl) {
        val start = builder.length
        builder.append(this.item.url)
        builder.setSpan(new TextAppearanceSpan(ProfileManagerActivity.this, android.R.style.TextAppearance_Small),
          start, builder.length, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
      }
      handler.post(() => text.setText(builder))
    }

    def bind(item: SSRSub) {
      this.item = item
      updateText()
    }

    def onClick(v: View) = {
      updateText(true)
    }

    def onKey(v: View, keyCode: Int, event: KeyEvent) = {
      true
    }
  }

  private class SSRSubAdapter extends RecyclerView.Adapter[SSRSubViewHolder] {
    var profiles = new ArrayBuffer[SSRSub]
    profiles ++= app.ssrsubManager.getAllSSRSubs.getOrElse(List.empty[SSRSub])

    def getItemCount = profiles.length

    def onBindViewHolder(vh: SSRSubViewHolder, i: Int) = vh.bind(profiles(i))

    def onCreateViewHolder(vg: ViewGroup, i: Int) =
      new SSRSubViewHolder(LayoutInflater.from(vg.getContext).inflate(R.layout.layout_ssr_sub_item, vg, false))

    def add(item: SSRSub) {
      undoManager.flush
      val pos = getItemCount
      profiles += item
      notifyItemInserted(pos)
    }

    def remove(pos: Int) {
      profiles.remove(pos)
      notifyItemRemoved(pos)
    }
  }

  private var selectedItem: ProfileViewHolder = _
  private val handler = new Handler

  private var menu : FloatingActionMenu = _

  private lazy val profilesAdapter = new ProfilesAdapter
  private lazy val ssrsubAdapter = new SSRSubAdapter
  private var undoManager: UndoSnackbarManager[Profile] = _

  private lazy val clipboard = getSystemService(Context.CLIPBOARD_SERVICE).asInstanceOf[ClipboardManager]

  private var nfcAdapter : NfcAdapter = _
  private var nfcShareItem: Array[Byte] = _
  private var isNfcAvailable: Boolean = _
  private var isNfcEnabled: Boolean = _
  private var isNfcBeamEnabled: Boolean = _

  private var testProgressDialog: ProgressDialog = _
  private var testAsyncJob: Thread = _
  private var isTesting: Boolean = true
  private var ssTestProcess: GuardedProcess = _

  private val REQUEST_QRCODE = 1
  private var is_sort: Boolean = false


  def isPortAvailable (port: Int):Boolean = {
    // Assume no connection is possible.
    var result = true;

    try {
      (new Socket("127.0.0.1", port)).close()
      result = false;
    } catch {
      case e: Exception => Unit
    }

    return result;
  }

  override def onCreate(savedInstanceState: Bundle) {
    super.onCreate(savedInstanceState)

    val action = getIntent().getAction()
    if (action != null && action.equals(Action.SCAN)) {
       qrcodeScan()
    }

    if (action != null && action.equals(Action.SORT)) {
       is_sort = true
    }

    getWindow.setFlags(WindowManager.LayoutParams.FLAG_SECURE, WindowManager.LayoutParams.FLAG_SECURE)
    getWindow.addFlags(WindowManager.LayoutParams.FLAG_SECURE)
    setContentView(R.layout.layout_profiles)

    val toolbar = findViewById(R.id.toolbar).asInstanceOf[Toolbar]
    toolbar.setTitle(R.string.profiles)
    toolbar.setNavigationIcon(R.drawable.ic_navigation_close)
    toolbar.setNavigationOnClickListener(_ => {
      val intent = getParentActivityIntent
      if (shouldUpRecreateTask(intent) || isTaskRoot)
        TaskStackBuilder.create(this).addNextIntentWithParentStack(intent).startActivities()
      else finish()
    })
    toolbar.inflateMenu(R.menu.profile_manager_menu)
    toolbar.setOnMenuItemClickListener(this)

    initFab()

    app.profileManager.setProfileAddedListener(profilesAdapter.add)
    val profilesList = findViewById(R.id.profilesList).asInstanceOf[RecyclerView]
    val layoutManager = new LinearLayoutManager(this)
    profilesList.setLayoutManager(layoutManager)
    profilesList.setItemAnimator(new DefaultItemAnimator)
    profilesList.setAdapter(profilesAdapter)
    layoutManager.scrollToPosition(profilesAdapter.profiles.zipWithIndex.collectFirst {
      case (profile, i) if profile.id == app.profileId => i
    }.getOrElse(-1))
    undoManager = new UndoSnackbarManager[Profile](profilesList, profilesAdapter.undo, profilesAdapter.commit)
    if (is_sort == false) {
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
    }

    attachService(new IShadowsocksServiceCallback.Stub {
      def stateChanged(state: Int, profileName: String, msg: String) = () // ignore
      def trafficUpdated(txRate: Long, rxRate: Long, txTotal: Long, rxTotal: Long) =
        if (selectedItem != null) selectedItem.updateText(txTotal, rxTotal)
    })

    if (app.settings.getBoolean(Key.profileTip, true)) {
      app.editor.putBoolean(Key.profileTip, false).apply
      new AlertDialog.Builder(this, R.style.Theme_Material_Dialog_Alert).setTitle(R.string.profile_manager_dialog)
        .setMessage(R.string.profile_manager_dialog_content).setPositiveButton(R.string.gotcha, null).create.show
    }

    val intent = getIntent
    if (intent != null) handleShareIntent(intent)
  }

  def initFab() {
    menu = findViewById(R.id.menu).asInstanceOf[FloatingActionMenu]
    menu.setClosedOnTouchOutside(true)
    val dm = AppCompatDrawableManager.get
    val manualAddFAB = findViewById(R.id.fab_manual_add).asInstanceOf[FloatingActionButton]
    manualAddFAB.setImageDrawable(dm.getDrawable(this, R.drawable.ic_content_create))
    manualAddFAB.setOnClickListener(this)
    val qrcodeAddFAB = findViewById(R.id.fab_qrcode_add).asInstanceOf[FloatingActionButton]
    qrcodeAddFAB.setImageDrawable(dm.getDrawable(this, R.drawable.ic_image_camera_alt))
    qrcodeAddFAB.setOnClickListener(this)
    val nfcAddFAB = findViewById(R.id.fab_nfc_add).asInstanceOf[FloatingActionButton]
    nfcAddFAB.setImageDrawable(dm.getDrawable(this, R.drawable.ic_device_nfc))
    nfcAddFAB.setOnClickListener(this)
    val importAddFAB = findViewById(R.id.fab_import_add).asInstanceOf[FloatingActionButton]
    importAddFAB.setImageDrawable(dm.getDrawable(this, R.drawable.ic_content_paste))
    importAddFAB.setOnClickListener(this)
    val ssrsubAddFAB = findViewById(R.id.fab_ssr_sub).asInstanceOf[FloatingActionButton]
    ssrsubAddFAB.setImageDrawable(dm.getDrawable(this, R.drawable.ic_rss))
    ssrsubAddFAB.setOnClickListener(this)
    menu.setOnMenuToggleListener(opened => if (opened) qrcodeAddFAB.setVisibility(
      if (getPackageManager.hasSystemFeature(PackageManager.FEATURE_CAMERA)) View.VISIBLE else View.GONE))
  }


  override def onResume() {
    super.onResume()
    updateNfcState()
  }

  override def onNewIntent(intent: Intent) {
    super.onNewIntent(intent)
    handleShareIntent(intent)
  }

  def qrcodeScan() {
    try {
        val intent = new Intent("com.google.zxing.client.android.SCAN")
        intent.putExtra("SCAN_MODE", "QR_CODE_MODE")

        startActivityForResult(intent, 0)
    } catch {
        case _ : Throwable =>
            /*val dialog = new AlertDialog.Builder(this, R.style.Theme_Material_Dialog_Alert)
              .setTitle(R.string.scan_qrcode_install_title)
              .setPositiveButton(android.R.string.yes, ((_, _) => {
                  val marketUri = Uri.parse("market://details?id=com.google.zxing.client.android")
                  val marketIntent = new Intent(Intent.ACTION_VIEW, marketUri)
                  startActivity(marketIntent)
                }
              ): DialogInterface.OnClickListener)
              .setNeutralButton(R.string.scan_qrcode_direct_download_text, ((_, _) => {
                  val marketUri = Uri.parse("https://breakwa11.github.io/download/BarcodeScanner.apk")
                  val marketIntent = new Intent(Intent.ACTION_VIEW, marketUri)
                  startActivity(marketIntent)
                }
              ): DialogInterface.OnClickListener)
              .setNegativeButton(android.R.string.no, ((_, _) => finish()): DialogInterface.OnClickListener)
              .setMessage(R.string.scan_qrcode_install_text)
              .create()
            dialog.show()*/
            menu.toggle(false)
            startActivity(new Intent(this, classOf[ScannerActivity]))
    }
  }

  override def onClick(v: View){
    v.getId match {
      case R.id.fab_manual_add =>
        menu.toggle(true)
        val profile = app.profileManager.createProfile()
        app.profileManager.updateProfile(profile)
        app.switchProfile(profile.id)
        finish
      case R.id.fab_qrcode_add =>
        menu.toggle(false)
        qrcodeScan()
      case R.id.fab_nfc_add =>
        menu.toggle(true)
        val dialog = new AlertDialog.Builder(ProfileManagerActivity.this, R.style.Theme_Material_Dialog_Alert)
          .setCancelable(true)
          .setPositiveButton(R.string.gotcha, null)
          .setTitle(R.string.add_profile_nfc_hint_title)
          .create()
        if (!isNfcBeamEnabled) {
          dialog.setMessage(getString(R.string.share_message_nfc_disabled))
          dialog.setButton(DialogInterface.BUTTON_NEUTRAL, getString(R.string.turn_on_nfc), ((_, _) =>
              startActivity(new Intent(Settings.ACTION_NFC_SETTINGS))
            ): DialogInterface.OnClickListener)
        } else {
          dialog.setMessage(getString(R.string.add_profile_nfc_hint))
        }
        dialog.show
      case R.id.fab_import_add =>
        menu.toggle(true)
        if (clipboard.hasPrimaryClip) {
          val profiles_normal = Parser.findAll(clipboard.getPrimaryClip.getItemAt(0).getText).toList
          val profiles_ssr = Parser.findAll_ssr(clipboard.getPrimaryClip.getItemAt(0).getText).toList
          val profiles = profiles_normal ::: profiles_ssr
          if (profiles.nonEmpty) {
            val dialog = new AlertDialog.Builder(this, R.style.Theme_Material_Dialog_Alert)
              .setTitle(R.string.add_profile_dialog)
              .setPositiveButton(android.R.string.yes, ((_, _) =>
                profiles.foreach(app.profileManager.createProfile)): DialogInterface.OnClickListener)
              .setNeutralButton(R.string.dr, ((_, _) =>
                profiles.foreach(app.profileManager.createProfile_dr)): DialogInterface.OnClickListener)
              .setNegativeButton(android.R.string.no, ((_, _) => finish()): DialogInterface.OnClickListener)
              .setMessage(profiles.mkString("\n"))
              .create()
            dialog.show()
            return
          }
        }
        Toast.makeText(this, R.string.action_import_err, Toast.LENGTH_SHORT).show
      case R.id.fab_ssr_sub =>
        menu.toggle(true)
        ssrsubDialog()
    }
  }

  def ssrsubDialog() {
    val prefs = PreferenceManager.getDefaultSharedPreferences(this)

    val view = View.inflate(this, R.layout.layout_ssr_sub, null);
    val sw_ssr_sub_autoupdate_enable = view.findViewById(R.id.sw_ssr_sub_autoupdate_enable).asInstanceOf[Switch]

    app.ssrsubManager.setSSRSubAddedListener(ssrsubAdapter.add)
    val ssusubsList = view.findViewById(R.id.ssrsubList).asInstanceOf[RecyclerView]
    val layoutManager = new LinearLayoutManager(this)
    ssusubsList.setLayoutManager(layoutManager)
    ssusubsList.setItemAnimator(new DefaultItemAnimator)
    ssusubsList.setAdapter(ssrsubAdapter)
    new ItemTouchHelper(new SimpleCallback(ItemTouchHelper.UP | ItemTouchHelper.DOWN,
      ItemTouchHelper.START | ItemTouchHelper.END) {
      def onSwiped(viewHolder: ViewHolder, direction: Int) = {
        val index = viewHolder.getAdapterPosition
        new AlertDialog.Builder(ProfileManagerActivity.this)
          .setTitle(getString(R.string.ssrsub_remove_tip_title))
          .setPositiveButton(R.string.ssrsub_remove_tip_direct, ((_, _) => {
            ssrsubAdapter.remove(index)
            app.ssrsubManager.delSSRSub(viewHolder.asInstanceOf[SSRSubViewHolder].item.id)
          }): DialogInterface.OnClickListener)
          .setNegativeButton(android.R.string.no,  ((_, _) => {
            ssrsubAdapter.notifyDataSetChanged()
          }): DialogInterface.OnClickListener)
          .setNeutralButton(R.string.ssrsub_remove_tip_delete,  ((_, _) => {
            var delete_profiles = app.profileManager.getAllProfilesByGroup(viewHolder.asInstanceOf[SSRSubViewHolder].item.url_group) match {
              case Some(profiles) =>
                profiles
              case _ => null
            }

            delete_profiles.foreach((profile: Profile) => {
              if (profile.id != app.profileId) {
                app.profileManager.delProfile(profile.id)
              }
            })

            val index = viewHolder.getAdapterPosition
            ssrsubAdapter.remove(index)
            app.ssrsubManager.delSSRSub(viewHolder.asInstanceOf[SSRSubViewHolder].item.id)

            finish()
            startActivity(new Intent(getIntent()))
          }): DialogInterface.OnClickListener)
          .setMessage(getString(R.string.ssrsub_remove_tip))
          .setCancelable(false)
          .create()
          .show()
      }
      def onMove(recyclerView: RecyclerView, viewHolder: ViewHolder, target: ViewHolder) = {
        true
      }
    }).attachToRecyclerView(ssusubsList)

    if (prefs.getInt(Key.ssrsub_autoupdate, 0) == 1) {
      sw_ssr_sub_autoupdate_enable.setChecked(true)
    }

    sw_ssr_sub_autoupdate_enable.setOnCheckedChangeListener(((_, isChecked: Boolean) => {
      val prefs_edit = prefs.edit()
      if (isChecked) {
        prefs_edit.putInt(Key.ssrsub_autoupdate, 1)
      } else {
        prefs_edit.putInt(Key.ssrsub_autoupdate, 0)
      }
      prefs_edit.apply()
    }): CompoundButton.OnCheckedChangeListener)

    new AlertDialog.Builder(this)
      .setTitle(getString(R.string.add_profile_methods_ssr_sub))
      .setPositiveButton(R.string.ssrsub_ok, ((_, _) => {
        Utils.ThrowableFuture {
          handler.post(() => {
            testProgressDialog = ProgressDialog.show(ProfileManagerActivity.this, getString(R.string.ssrsub_progres), getString(R.string.ssrsub_progres_text), false, true)
          })
          app.ssrsubManager.getAllSSRSubs match {
            case Some(ssrsubs) =>
              ssrsubs.foreach((ssrsub: SSRSub) => {

                  var delete_profiles = app.profileManager.getAllProfilesByGroup(ssrsub.url_group) match {
                    case Some(profiles) =>
                      profiles
                    case _ => null
                  }
                  var result = ""
                  val builder = new OkHttpClient.Builder()
                                  .connectTimeout(5, TimeUnit.SECONDS)
                                  .writeTimeout(5, TimeUnit.SECONDS)
                                  .readTimeout(5, TimeUnit.SECONDS)

                  val client = builder.build();

                  val request = new Request.Builder()
                    .url(ssrsub.url)
                    .build();

                  try {
                    val response = client.newCall(request).execute()
                    val code = response.code()
                    if (code == 200) {
                      val response_string = new String(Base64.decode(response.body().string, Base64.URL_SAFE))
                      var limit_num = -1
                      var encounter_num = 0
                      if (response_string.indexOf("MAX=") == 0) {
                        limit_num = response_string.split("\\n")(0).split("MAX=")(1).replaceAll("\\D+","").toInt
                      }
                      var profiles_ssr = Parser.findAll_ssr(response_string)
                      profiles_ssr = scala.util.Random.shuffle(profiles_ssr)
                      profiles_ssr.foreach((profile: Profile) => {
                        if (encounter_num < limit_num && limit_num != -1 || limit_num == -1) {
                          val result = app.profileManager.createProfile_sub(profile)
                          if (result != 0) {
                            delete_profiles = delete_profiles.filter(_.id != result)
                          }
                        }
                        encounter_num += 1
                      })

                      delete_profiles.foreach((profile: Profile) => {
                        if (profile.id != app.profileId) {
                          app.profileManager.delProfile(profile.id)
                        }
                      })
                    } else throw new Exception(getString(R.string.ssrsub_error, code: Integer))
                    response.body().close()
                  } catch {
                    case e: IOException =>
                      result = getString(R.string.ssrsub_error, e.getMessage)
                  }
              })
            case _ => Toast.makeText(this, R.string.action_export_err, Toast.LENGTH_SHORT).show
          }

          handler.post(() => testProgressDialog.dismiss)

          finish()
          startActivity(new Intent(getIntent()))
        }
      }): DialogInterface.OnClickListener)
      .setNegativeButton(android.R.string.no, null)
      .setNeutralButton(R.string.ssrsub_add, ((_, _) => {
        val UrlAddEdit = new EditText(this);
        new AlertDialog.Builder(this)
          .setTitle(getString(R.string.ssrsub_add))
          .setPositiveButton(android.R.string.ok, ((_, _) => {
            if(UrlAddEdit.getText().toString() != "") {
              Utils.ThrowableFuture {
                handler.post(() => {
                  testProgressDialog = ProgressDialog.show(ProfileManagerActivity.this, getString(R.string.ssrsub_progres), getString(R.string.ssrsub_progres_text), false, true)
                })
                var result = ""
                val builder = new OkHttpClient.Builder()
                                .connectTimeout(5, TimeUnit.SECONDS)
                                .writeTimeout(5, TimeUnit.SECONDS)
                                .readTimeout(5, TimeUnit.SECONDS)

                val client = builder.build();

                try {
                  val request = new Request.Builder()
                    .url(UrlAddEdit.getText().toString())
                    .build();
                  val response = client.newCall(request).execute()
                  val code = response.code()
                  if (code == 200) {
                    val profiles_ssr = Parser.findAll_ssr(new String(Base64.decode(response.body().string, Base64.URL_SAFE))).toList
                    if(profiles_ssr(0).url_group != "") {
                      val ssrsub = new SSRSub {
                        url = UrlAddEdit.getText().toString()
                        url_group = profiles_ssr(0).url_group
                      }
                      handler.post(() => app.ssrsubManager.createSSRSub(ssrsub))
                    }
                  } else throw new Exception(getString(R.string.ssrsub_error, code: Integer))
                  response.body().close()
                } catch {
                  case e: Exception =>
                    result = getString(R.string.ssrsub_error, e.getMessage)
                }
                handler.post(() => testProgressDialog.dismiss)
                handler.post(() => ssrsubDialog())
              }
            } else {
              handler.post(() => ssrsubDialog())
            }
          }): DialogInterface.OnClickListener)
          .setNegativeButton(android.R.string.no, ((_, _) => {
            ssrsubDialog()
          }): DialogInterface.OnClickListener)
          .setView(UrlAddEdit)
          .create()
          .show()
      }): DialogInterface.OnClickListener)
      .setView(view)
      .create()
      .show()
  }

  def updateNfcState() {
    isNfcAvailable = false
    isNfcEnabled = false
    isNfcBeamEnabled = false
    nfcAdapter = NfcAdapter.getDefaultAdapter(this)
    if (nfcAdapter != null) {
      isNfcAvailable = true
      if (nfcAdapter.isEnabled) {
        isNfcEnabled = true
        if (nfcAdapter.isNdefPushEnabled) {
          isNfcBeamEnabled = true
          nfcAdapter.setNdefPushMessageCallback(null, ProfileManagerActivity.this)
        }
      }
    }
  }

  def handleShareIntent(intent: Intent) {
    val sharedStr = intent.getAction match {
      case Intent.ACTION_VIEW => intent.getData.toString
      case NfcAdapter.ACTION_NDEF_DISCOVERED =>
        val rawMsgs = intent.getParcelableArrayExtra(NfcAdapter.EXTRA_NDEF_MESSAGES)
        if (rawMsgs != null && rawMsgs.nonEmpty)
          new String(rawMsgs(0).asInstanceOf[NdefMessage].getRecords()(0).getPayload)
        else null
      case _ => null
    }

    if (TextUtils.isEmpty(sharedStr)) return
    val profiles_normal = Parser.findAll(sharedStr).toList
    val profiles_ssr = Parser.findAll_ssr(sharedStr).toList
    val profiles = profiles_ssr ::: profiles_normal
    if (profiles.isEmpty) {
      finish()
      return
    }
    val dialog = new AlertDialog.Builder(this, R.style.Theme_Material_Dialog_Alert)
      .setTitle(R.string.add_profile_dialog)
      .setPositiveButton(android.R.string.yes, ((_, _) =>
        profiles.foreach(app.profileManager.createProfile)): DialogInterface.OnClickListener)
      .setNeutralButton(R.string.dr, ((_, _) =>
        profiles.foreach(app.profileManager.createProfile_dr)): DialogInterface.OnClickListener)
      .setNegativeButton(android.R.string.no, ((_, _) => finish()): DialogInterface.OnClickListener)
      .setMessage(profiles.mkString("\n"))
      .create()
    dialog.show()
  }

  override def onActivityResult(requestCode: Int, resultCode: Int, data: Intent) {
      if (requestCode == 0) {
          if (resultCode == Activity.RESULT_OK) {
              val contents = data.getStringExtra("SCAN_RESULT")
              if (TextUtils.isEmpty(contents)) return
              val profiles_normal = Parser.findAll(contents).toList
              val profiles_ssr = Parser.findAll_ssr(contents).toList
              val profiles = profiles_ssr ::: profiles_normal
              if (profiles.isEmpty) {
                finish()
                return
              }
              val dialog = new AlertDialog.Builder(this, R.style.Theme_Material_Dialog_Alert)
                .setTitle(R.string.add_profile_dialog)
                .setPositiveButton(android.R.string.yes, ((_, _) =>
                  profiles.foreach(app.profileManager.createProfile)): DialogInterface.OnClickListener)
                .setNeutralButton(R.string.dr, ((_, _) =>
                  profiles.foreach(app.profileManager.createProfile_dr)): DialogInterface.OnClickListener)
                .setNegativeButton(android.R.string.no, ((_, _) => finish()): DialogInterface.OnClickListener)
                .setMessage(profiles.mkString("\n"))
                .create()
              dialog.show()
          }
          if(resultCode == Activity.RESULT_CANCELED){
              //handle cancel
          }
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
    detachService()

    if (ssTestProcess != null) {
      ssTestProcess.destroy()
      ssTestProcess = null
    }

    undoManager.flush
    app.profileManager.setProfileAddedListener(null)
    super.onDestroy
  }

  override def onBackPressed() {
    if (menu.isOpened) menu.close(true) else super.onBackPressed()
  }

  def createNdefMessage(nfcEvent: NfcEvent) =
    new NdefMessage(Array(new NdefRecord(NdefRecord.TNF_ABSOLUTE_URI, nfcShareItem, Array[Byte](), nfcShareItem)))

  val showProgresshandler = new Handler(Looper.getMainLooper()) {
    override def handleMessage(msg: Message) {
      val message = msg.obj.asInstanceOf[String]
      if (testProgressDialog != null) {
        testProgressDialog.setMessage(message)
      }
    }
  }

  def onMenuItemClick(item: MenuItem): Boolean = item.getItemId match {
    case R.id.action_export =>
      app.profileManager.getAllProfiles match {
        case Some(profiles) =>
          clipboard.setPrimaryClip(ClipData.newPlainText(null, profiles.mkString("\n")))
          Toast.makeText(this, R.string.action_export_msg, Toast.LENGTH_SHORT).show
        case _ => Toast.makeText(this, R.string.action_export_err, Toast.LENGTH_SHORT).show
      }
      true
    case R.id.action_full_test =>
      app.profileManager.getAllProfiles match {
        case Some(profiles) =>

          isTesting = true

          testProgressDialog = ProgressDialog.show(this, getString(R.string.tips_testing), getString(R.string.tips_testing), false, true, new OnCancelListener() {
              def onCancel(dialog: DialogInterface) {
                  // TODO Auto-generated method stub
                  // Do something...
                  if (testProgressDialog != null) {
                    testProgressDialog = null;
                  }

                  isTesting = false
                  testAsyncJob.interrupt()

                  finish()
                  startActivity(new Intent(getIntent()))
              }
          })

          testAsyncJob = new Thread {
            override def run() {
              // Do some background work
              Looper.prepare()
              profiles.foreach((profile: Profile) => {
                if (isTesting) {

                  if (testAsyncJob.isInterrupted()) {
                    isTesting = false
                  }
                  // Resolve the server address
                  var host = profile.host
                  if (!Utils.isNumeric(host)) Utils.resolve(host, enableIPv6 = true) match {
                    case Some(addr) => host = addr
                    case None => throw new Exception("can't resolve")
                  }

                  val conf = ConfigUtils
                    .SHADOWSOCKS.formatLocal(Locale.ENGLISH, host, profile.remotePort, profile.localPort + 2,
                      ConfigUtils.EscapedJson(profile.password), profile.method, 600, profile.protocol, profile.obfs, ConfigUtils.EscapedJson(profile.obfs_param), ConfigUtils.EscapedJson(profile.protocol_param))
                  Utils.printToFile(new File(getApplicationInfo.dataDir + "/ss-local-test.conf"))(p => {
                    p.println(conf)
                  })

                  val cmd = ArrayBuffer[String](getApplicationInfo.dataDir + "/ss-local"
                    , "-t", "600"
                    , "-L", "www.google.com:80"
                    , "-c", getApplicationInfo.dataDir + "/ss-local-test.conf")

                  if (TcpFastOpen.sendEnabled) cmd += "--fast-open"

                  if (ssTestProcess != null) {
                    ssTestProcess.destroy()
                    ssTestProcess = null
                  }

                  ssTestProcess = new GuardedProcess(cmd).start()

                  val start = currentTimeMillis
                  while (start - currentTimeMillis < 5 * 1000 && isPortAvailable(profile.localPort + 2)) {
                    try {
                      Thread.sleep(50)
                    } catch{
                      case e: InterruptedException => isTesting = false
                    }
                  }

                  var result = ""
                  val builder = new OkHttpClient.Builder()
                                  .connectTimeout(5, TimeUnit.SECONDS)
                                  .writeTimeout(5, TimeUnit.SECONDS)
                                  .readTimeout(5, TimeUnit.SECONDS)

                  val client = builder.build();

                  val request = new Request.Builder()
                    .url("http://127.0.0.1:" + (profile.localPort + 2) + "/generate_204").removeHeader("Host").addHeader("Host", "www.google.com")
                    .build();

                  try {
                    val response = client.newCall(request).execute()
                    val code = response.code()
                    if (code == 204 || code == 200 && response.body().contentLength == 0) {
                      val start = currentTimeMillis
                      val response = client.newCall(request).execute()
                      val elapsed = currentTimeMillis - start
                      val code = response.code()
                      if (code == 204 || code == 200 && response.body().contentLength == 0)
                      {
                        result = getString(R.string.connection_test_available, elapsed: java.lang.Long)
                        profile.elapsed = elapsed
                        app.profileManager.updateProfile(profile)
                      }
                      else throw new Exception(getString(R.string.connection_test_error_status_code, code: Integer))
                      response.body().close()
                    } else throw new Exception(getString(R.string.connection_test_error_status_code, code: Integer))
                    response.body().close()
                  } catch {
                    case e: IOException =>
                      result = getString(R.string.connection_test_error, e.getMessage)
                  }

                  var msg = Message.obtain()
                  msg.obj = profile.name + " " + result
                  msg.setTarget(showProgresshandler)
                  msg.sendToTarget()

                  //val proxy = new Proxy(Proxy.Type.SOCKS, new InetSocketAddress("127.0.0.1", profile.localPort + 2))

                  // Based on: https://android.googlesource.com/platform/frameworks/base/+/master/services/core/java/com/android/server/connectivity/NetworkMonitor.java#640
                  /*autoDisconnect(new URL("https", "www.google.com", "/generate_204").openConnection(proxy)
                    .asInstanceOf[HttpURLConnection]) { conn =>
                    conn.setConnectTimeout(5 * 1000)
                    conn.setReadTimeout(5 * 1000)
                    conn.setInstanceFollowRedirects(false)
                    conn.setUseCaches(false)
                    var result: String = null
                    var success = true
                    try {
                      conn.getInputStream
                      val code = conn.getResponseCode
                      if (code == 204 || code == 200 && conn.getContentLength == 0)
                      {
                        autoDisconnect(new URL("https", "www.google.com", "/generate_204").openConnection(proxy)
                          .asInstanceOf[HttpURLConnection]) { conn =>
                          conn.setConnectTimeout(5 * 1000)
                          conn.setReadTimeout(5 * 1000)
                          conn.setInstanceFollowRedirects(false)
                          conn.setUseCaches(false)
                          var result: String = null
                          var success = true
                          try {
                            val start = currentTimeMillis
                            conn.getInputStream
                            val elapsed = currentTimeMillis - start
                            val code = conn.getResponseCode
                            if (code == 204 || code == 200 && conn.getContentLength == 0)
                            {
                              result = getString(R.string.connection_test_available, elapsed: java.lang.Long)
                              profile.elapsed = elapsed
                              app.profileManager.updateProfile(profile)
                            }
                            else throw new Exception(getString(R.string.connection_test_error_status_code, code: Integer))
                          } catch {
                            case e: Exception =>
                              success = false
                              result = getString(R.string.connection_test_error, e.getMessage)
                          }

                          var msg = Message.obtain()
                          msg.obj = profile.name + " " + result
                          msg.setTarget(showProgresshandler)
                          msg.sendToTarget()
                        }
                      }
                      else throw new Exception(getString(R.string.connection_test_error_status_code, code: Integer))
                    } catch {
                      case e: Exception =>
                        success = false
                        result = getString(R.string.connection_test_error, e.getMessage)
                        var msg = Message.obtain()
                        msg.obj = profile.name + " " + result;
                        msg.setTarget(showProgresshandler)
                        msg.sendToTarget()
                    }
                  }*/

                  if (ssTestProcess != null) {
                    ssTestProcess.destroy()
                    ssTestProcess = null
                  }
                }
              })

              if (testProgressDialog != null) {
                testProgressDialog.dismiss
                testProgressDialog = null;
              }

              finish()
              startActivity(new Intent(getIntent()))
              Looper.loop()
            }
          }

          testAsyncJob.start()

        case _ => Toast.makeText(this, R.string.action_export_err, Toast.LENGTH_SHORT).show
      }
      true
    case R.id.action_sort =>
      finish()
      val intent = new Intent(Action.SORT)
      startActivity(intent)
      true
    case _ => false
  }
}
