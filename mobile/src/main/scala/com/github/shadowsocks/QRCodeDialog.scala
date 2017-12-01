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

import java.nio.charset.Charset

import android.app.Activity
import android.content.Context
import android.nfc.{NdefMessage, NdefRecord, NfcAdapter}
import android.os.Bundle
import android.support.v4.app.DialogFragment
import android.view.{LayoutInflater, View, ViewGroup}
import android.widget.{ImageView, LinearLayout}
import net.glxn.qrgen.android.QRCode

object QRCodeDialog {
  private final val KEY_URL = "com.github.shadowsocks.QRCodeDialog.KEY_URL"
}

final class QRCodeDialog extends DialogFragment {
  import QRCodeDialog._
  def this(url: String) {
    this()
    val bundle = new Bundle()
    bundle.putString(KEY_URL, url)
    setArguments(bundle)
  }
  private def url = getArguments.getString(KEY_URL)

  private lazy val nfcShareItem = url.getBytes(Charset.forName("UTF-8"))
  private var adapter: NfcAdapter = _

  override def onCreateView(inflater: LayoutInflater, container: ViewGroup, savedInstanceState: Bundle): View = {
    val image = new ImageView(getActivity)
    image.setLayoutParams(new LinearLayout.LayoutParams(-1, -1))
    val size = getResources.getDimensionPixelSize(R.dimen.qr_code_size)
    val qrcode = QRCode.from(url)
      .withSize(size, size)
      .asInstanceOf[QRCode].bitmap()
    image.setImageBitmap(qrcode)
    image
  }

  override def onAttach(context: Context) {
    super.onAttach(context)
    adapter = NfcAdapter.getDefaultAdapter(getActivity)
    if (adapter != null) adapter.setNdefPushMessage(new NdefMessage(Array(
      new NdefRecord(NdefRecord.TNF_ABSOLUTE_URI, nfcShareItem, Array[Byte](), nfcShareItem))),
      context.asInstanceOf[Activity])
  }

  override def onDetach() {
    if (adapter != null) {
      adapter.setNdefPushMessage(null, getActivity)
      adapter = null
    }
    super.onDetach()
  }
}
