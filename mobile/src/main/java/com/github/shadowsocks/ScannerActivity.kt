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

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.content.pm.ShortcutManager
import android.graphics.BitmapFactory
import android.hardware.camera2.CameraAccessException
import android.hardware.camera2.CameraManager
import android.os.Build
import android.os.Bundle
import android.support.v4.app.TaskStackBuilder
import android.support.v7.app.AppCompatActivity
import android.support.v7.widget.Toolbar
import android.util.Log
import android.util.SparseArray
import android.view.MenuItem
import android.widget.Toast
import com.github.shadowsocks.App.Companion.app
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.database.ProfileManager
import com.github.shadowsocks.utils.resolveResourceId
import com.google.android.gms.samples.vision.barcodereader.BarcodeCapture
import com.google.android.gms.samples.vision.barcodereader.BarcodeGraphic
import com.google.android.gms.vision.Frame
import com.google.android.gms.vision.barcode.Barcode
import com.google.android.gms.vision.barcode.BarcodeDetector
import xyz.belvi.mobilevisionbarcodescanner.BarcodeRetriever

class ScannerActivity : AppCompatActivity(), Toolbar.OnMenuItemClickListener, BarcodeRetriever {
    companion object {
        private const val TAG = "ScannerActivity"
        private const val REQUEST_IMPORT = 2
        private const val REQUEST_IMPORT_OR_FINISH = 3
    }

    private fun navigateUp() {
        val intent = parentActivityIntent
        if (shouldUpRecreateTask(intent) || isTaskRoot)
            TaskStackBuilder.create(this).addNextIntentWithParentStack(intent).startActivities()
        else finish()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        if (Build.VERSION.SDK_INT >= 25) getSystemService(ShortcutManager::class.java).reportShortcutUsed("scan")
        if (try {
                    (getSystemService(Context.CAMERA_SERVICE) as CameraManager).cameraIdList.isEmpty()
                } catch (_: CameraAccessException) {
                    true
                }) {
            startImport()
            return
        }
        setContentView(R.layout.layout_scanner)
        val toolbar = findViewById<Toolbar>(R.id.toolbar)
        toolbar.title = title
        toolbar.setNavigationIcon(theme.resolveResourceId(R.attr.homeAsUpIndicator))
        toolbar.setNavigationOnClickListener { navigateUp() }
        toolbar.inflateMenu(R.menu.scanner_menu)
        toolbar.setOnMenuItemClickListener(this)
        (supportFragmentManager.findFragmentById(R.id.barcode) as BarcodeCapture).setRetrieval(this)
    }

    override fun onRetrieved(barcode: Barcode) = runOnUiThread {
        Profile.findAll(barcode.rawValue).forEach { ProfileManager.createProfile(it) }
        navigateUp()
    }
    override fun onRetrievedMultiple(closetToClick: Barcode?, barcode: MutableList<BarcodeGraphic>?) = check(false)
    override fun onBitmapScanned(sparseArray: SparseArray<Barcode>?) { }
    override fun onRetrievedFailed(reason: String?) {
        Log.w(TAG, reason)
    }
    override fun onPermissionRequestDenied() {
        Toast.makeText(this, R.string.add_profile_scanner_permission_required, Toast.LENGTH_SHORT).show()
        startImport()
    }

    override fun onMenuItemClick(item: MenuItem) = when (item.itemId) {
        R.id.action_import -> {
            startImport(true)
            true
        }
        else -> false
    }

    private fun startImport(manual: Boolean = false) = startActivityForResult(Intent(Intent.ACTION_OPEN_DOCUMENT)
            .addCategory(Intent.CATEGORY_OPENABLE)
            .setType("image/*")
            .putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true), if (manual) REQUEST_IMPORT else REQUEST_IMPORT_OR_FINISH)
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        when (requestCode) {
            REQUEST_IMPORT, REQUEST_IMPORT_OR_FINISH -> if (resultCode == Activity.RESULT_OK) {
                var success = false
                val detector = BarcodeDetector.Builder(this)
                        .setBarcodeFormats(Barcode.QR_CODE)
                        .build()
                if (detector.isOperational) {
                    var list = listOfNotNull(data?.data)
                    val clipData = data?.clipData
                    if (clipData != null) list += (0 until clipData.itemCount).map { clipData.getItemAt(it).uri }
                    val resolver = contentResolver
                    for (uri in list) try {
                        val barcodes = detector.detect(Frame.Builder()
                                .setBitmap(BitmapFactory.decodeStream(resolver.openInputStream(uri))).build())
                        for (i in 0 until barcodes.size()) Profile.findAll(barcodes.valueAt(i).rawValue).forEach {
                            ProfileManager.createProfile(it)
                            success = true
                        }
                    } catch (e: Exception) {
                        app.track(e)
                    }
                } else Log.w(TAG, "Google vision isn't operational.")
                Toast.makeText(this, if (success) R.string.action_import_msg else R.string.action_import_err,
                        Toast.LENGTH_SHORT).show()
                navigateUp()
            } else if (requestCode == REQUEST_IMPORT_OR_FINISH) navigateUp()
            else -> super.onActivityResult(requestCode, resultCode, data)
        }
    }
}
