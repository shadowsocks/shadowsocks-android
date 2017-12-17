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

import android.content.pm.PackageManager
import android.content.pm.ShortcutManager
import android.os.Build
import android.os.Bundle
import android.support.v4.app.ActivityCompat
import android.support.v4.app.TaskStackBuilder
import android.support.v4.content.ContextCompat
import android.support.v7.app.AppCompatActivity
import android.support.v7.widget.Toolbar
import android.widget.Toast
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.database.ProfileManager
import com.github.shadowsocks.utils.resolveResourceId
import com.google.zxing.Result
import me.dm7.barcodescanner.zxing.ZXingScannerView

class ScannerActivity : AppCompatActivity(), ZXingScannerView.ResultHandler {
    companion object {
        private const val MY_PERMISSIONS_REQUEST_CAMERA = 1
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
        scannerView = findViewById(R.id.scanner)
        if (Build.VERSION.SDK_INT >= 25) getSystemService(ShortcutManager::class.java).reportShortcutUsed("scan")
    }

    override fun onResume() {
        super.onResume()
        val permissionCheck = ContextCompat.checkSelfPermission(this,
                android.Manifest.permission.CAMERA)
        if (permissionCheck == PackageManager.PERMISSION_GRANTED) {
            scannerView.setResultHandler(this)  // Register ourselves as a handler for scan results.
            scannerView.startCamera()           // Start camera on resume
        } else ActivityCompat.requestPermissions(this, arrayOf(android.Manifest.permission.CAMERA),
                MY_PERMISSIONS_REQUEST_CAMERA)
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        if (requestCode == MY_PERMISSIONS_REQUEST_CAMERA)
            if (grantResults.getOrNull(0) == PackageManager.PERMISSION_GRANTED) {
                scannerView.setResultHandler(this)
                scannerView.startCamera()
            } else {
                Toast.makeText(this, R.string.add_profile_scanner_permission_required, Toast.LENGTH_SHORT).show()
                finish()
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
}
