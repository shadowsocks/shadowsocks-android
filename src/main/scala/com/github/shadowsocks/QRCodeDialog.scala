package com.github.shadowsocks

import java.nio.charset.Charset

import android.app.Activity
import android.nfc.{NdefMessage, NdefRecord, NfcAdapter}
import android.os.Bundle
import android.view.{LayoutInflater, View, ViewGroup}
import android.widget.{ImageView, LinearLayout}
import com.github.shadowsocks.utils.Utils
import net.glxn.qrgen.android.QRCode

final class QRCodeDialog(url: String) extends DialogFragment {
  private lazy val nfcShareItem = url.getBytes(Charset.forName("UTF-8"))
  private var adapter: NfcAdapter = _

  override def onCreateView(inflater: LayoutInflater, container: ViewGroup, savedInstanceState: Bundle): View = {
    val image = new ImageView(getActivity)
    image.setLayoutParams(new LinearLayout.LayoutParams(-1, -1))
    val qrcode = QRCode.from(url)
      .withSize(Utils.dpToPx(getActivity, 250), Utils.dpToPx(getActivity, 250))
      .asInstanceOf[QRCode].bitmap()
    image.setImageBitmap(qrcode)
    image
  }

  override def onAttach(activity: Activity) {
    superOnAttach(activity)
    adapter = NfcAdapter.getDefaultAdapter(getActivity)
    if (adapter != null) adapter.setNdefPushMessage(new NdefMessage(Array(
      new NdefRecord(NdefRecord.TNF_ABSOLUTE_URI, nfcShareItem, Array[Byte](), nfcShareItem))), activity)
  }

  override def onDetach() {
    if (adapter != null) {
      adapter.setNdefPushMessage(null, getActivity)
      adapter = null
    }
    super.onDetach()
  }
}
