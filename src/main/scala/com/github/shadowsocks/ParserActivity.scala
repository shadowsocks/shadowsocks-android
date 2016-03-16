/*
 * Shadowsocks - A shadowsocks client for Android
 * Copyright (C) 2014 <max.c.lv@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *                            ___====-_  _-====___
 *                      _--^^^#####//      \\#####^^^--_
 *                   _-^##########// (    ) \\##########^-_
 *                  -############//  |\^^/|  \\############-
 *                _/############//   (@::@)   \\############\_
 *               /#############((     \\//     ))#############\
 *              -###############\\    (oo)    //###############-
 *             -#################\\  / VV \  //#################-
 *            -###################\\/      \//###################-
 *           _#/|##########/\######(   /\   )######/\##########|\#_
 *           |/ |#/\#/\#/\/  \#/\##\  |  |  /##/\#/  \/\#/\#/\#| \|
 *           `  |/  V  V  `   V  \#\| |  | |/#/  V   '  V  V  \|  '
 *              `   `  `      `   / | |  | | \   '      '  '   '
 *                               (  | |  | |  )
 *                              __\ | |  | | /__
 *                             (vvv(VVV)(VVV)vvv)
 *
 *                              HERE BE DRAGONS
 *
 */

package com.github.shadowsocks

import android.content.{Intent, DialogInterface}
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.nfc.{NdefMessage, NfcAdapter}
import android.os.{Parcelable, Bundle}
import android.support.v7.app.{AppCompatActivity, AlertDialog}
import android.view.WindowManager
import com.github.shadowsocks.utils.Parser

class ParserActivity extends AppCompatActivity {
  override def onCreate(savedInstanceState: Bundle) {
    super.onCreate(savedInstanceState)
  }


  override def onResume() {
    super.onResume()
    var sharedStr = ""
    if (NfcAdapter.ACTION_NDEF_DISCOVERED.equals(getIntent.getAction)) {
      val rawMsgs = getIntent.getParcelableArrayExtra(NfcAdapter.EXTRA_NDEF_MESSAGES)
      if (rawMsgs != null) {
        val msg = rawMsgs(0).asInstanceOf[NdefMessage]
        val records = msg.getRecords
        sharedStr = new String(records(0).getPayload)
      }
    } else if (Intent.ACTION_VIEW.equals(getIntent.getAction)) {
      sharedStr = getIntent.getData.toString
    }
    val profiles = Parser.findAll(sharedStr)
    if (profiles.isEmpty) {
      finish()
      return
    }
    showAsPopup()
    val dialog = new AlertDialog.Builder(this)
      .setTitle(R.string.add_profile_dialog)
      .setCancelable(false)
      .setPositiveButton(android.R.string.yes, ((_, _) =>
        profiles.foreach(ShadowsocksApplication.profileManager.createProfile)): DialogInterface.OnClickListener)
      .setNegativeButton(android.R.string.no, null)
      .setMessage(sharedStr)
      .create()
    dialog.setOnDismissListener((dialog: DialogInterface) => finish()) // builder method was added in API 17
    dialog.show()
  }

  def showAsPopup() {
    val window = getWindow
    window.setFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND, WindowManager.LayoutParams.FLAG_DIM_BEHIND)
    val params = window.getAttributes
    params.alpha = 1.0f
    params.dimAmount = 0.5f
    window.setAttributes(params.asInstanceOf[android.view.WindowManager.LayoutParams])
    window.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT))
  }
}
