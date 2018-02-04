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
import android.content.pm.PackageManager
import android.content.pm.ShortcutManager
import android.graphics.BitmapFactory
import android.hardware.camera2.CameraAccessException
import android.hardware.camera2.CameraManager
import android.os.Build
import android.os.Bundle
import android.support.v4.app.ActivityCompat
import android.support.v4.app.TaskStackBuilder
import android.support.v4.content.ContextCompat
import android.support.v7.app.AppCompatActivity
import android.support.v7.widget.Toolbar
import android.view.MenuItem
import android.widget.Toast
import com.github.shadowsocks.App.Companion.app
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.database.ProfileManager
import com.github.shadowsocks.utils.resolveResourceId
import com.google.zxing.BinaryBitmap
import com.google.zxing.MultiFormatReader
import com.google.zxing.RGBLuminanceSource
import com.google.zxing.Result
import com.google.zxing.common.HybridBinarizer
import me.dm7.barcodescanner.zxing.ZXingScannerView

class ScannerActivity : AppCompatActivity(), ZXingScannerView.ResultHandler, Toolbar.OnMenuItemClickListener {
    companion object {
        private const val MY_PERMISSIONS_REQUEST_CAMERA = 1
        private const val REQUEST_IMPORT = 2
        private const val REQUEST_IMPORT_OR_FINISH = 3
    }

    private lateinit var scannerView: ZXingScannerView

    private fun navigateUp() {
        val intent = parentActivityIntent
        if (shouldUpRecreateTask(intent) || isTaskRoot)
            TaskStackBuilder.create(this).addNextIntentWithParentStack(intent).startActivities()
        else finish()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.layout_scanner)
        val toolbar = findViewById<Toolbar>(R.id.toolbar)
        toolbar.title = title
        toolbar.setNavigationIcon(theme.resolveResourceId(R.attr.homeAsUpIndicator))
        toolbar.setNavigationOnClickListener { navigateUp() }
        toolbar.inflateMenu(R.menu.scanner_menu)
        toolbar.setOnMenuItemClickListener(this)
        scannerView = findViewById(R.id.scanner)
        if (Build.VERSION.SDK_INT >= 25) getSystemService(ShortcutManager::class.java).reportShortcutUsed("scan")
    }

    override fun onStart() {
        super.onStart()
        if (try {
                    (getSystemService(Context.CAMERA_SERVICE) as CameraManager).cameraIdList.isEmpty()
                } catch (_: CameraAccessException) {
                    true
                }) {
            startImport()
            return
        }
        val permissionCheck = ContextCompat.checkSelfPermission(this,
                android.Manifest.permission.CAMERA)
        if (permissionCheck != PackageManager.PERMISSION_GRANTED)
            ActivityCompat.requestPermissions(this, arrayOf(android.Manifest.permission.CAMERA),
                MY_PERMISSIONS_REQUEST_CAMERA)
    }

    override fun onResume() {
        super.onResume()
        val permissionCheck = ContextCompat.checkSelfPermission(this, android.Manifest.permission.CAMERA)
        if (permissionCheck == PackageManager.PERMISSION_GRANTED) {
            scannerView.setResultHandler(this)  // Register ourselves as a handler for scan results.
            scannerView.startCamera()           // Start camera on resume
        }
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        if (requestCode == MY_PERMISSIONS_REQUEST_CAMERA)
            if (grantResults.getOrNull(0) == PackageManager.PERMISSION_GRANTED) {
                scannerView.setResultHandler(this)
                scannerView.startCamera()
            } else {
                Toast.makeText(this, R.string.add_profile_scanner_permission_required, Toast.LENGTH_SHORT).show()
                startImport()
            }
        else super.onRequestPermissionsResult(requestCode, permissions, grantResults)
    }

    override fun onPause() {
        super.onPause()
        scannerView.stopCamera()    // Stop camera on pause
    }

    override fun handleResult(rawResult: Result?) {
        Profile.findAll(rawResult?.text).forEach { ProfileManager.createProfile(it) }
        navigateUp()
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
                var list = listOfNotNull(data?.data)
                val clipData = data?.clipData
                if (clipData != null) list += (0 until clipData.itemCount).map { clipData.getItemAt(it).uri }
                val resolver = contentResolver
                val reader = MultiFormatReader()
                var success = false
                for (uri in list) try {
                    val bitmap = BitmapFactory.decodeStream(resolver.openInputStream(uri))
                    val pixels = IntArray(bitmap.width * bitmap.height)
                    bitmap.getPixels(pixels, 0, bitmap.width, 0, 0, bitmap.width, bitmap.height)
                    Profile.findAll(reader.decode(BinaryBitmap(HybridBinarizer(
                            RGBLuminanceSource(bitmap.width, bitmap.height, pixels)))).text).forEach {
                        ProfileManager.createProfile(it)
                        success = true
                    }
                } catch (e: Exception) {
                    app.track(e)
                }
                Toast.makeText(this, if (success) R.string.action_import_msg else R.string.action_import_err,
                        Toast.LENGTH_SHORT).show()
                finish()
            } else if (requestCode == REQUEST_IMPORT_OR_FINISH) finish()
            else -> super.onActivityResult(requestCode, resultCode, data)
        }
    }
}
