package com.github.shadowsocks

import android.Manifest
import android.app.Activity
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Bundle
import android.support.v4.app.ActivityCompat
import android.support.v4.content.ContextCompat
import android.support.v7.app.AppCompatActivity
import android.support.v7.widget.Toolbar
import android.view.{MenuItem, ViewGroup}
import android.widget.Toast
import com.google.zxing.Result
import me.dm7.barcodescanner.zxing.ZXingScannerView

class ScannerActivity extends AppCompatActivity with ZXingScannerView.ResultHandler {

  val MY_PERMISSIONS_REQUEST_CAMERA = 1

  var scannerView: ZXingScannerView = _

  override def onRequestPermissionsResult(requestCode: Int, permissions: Array[String],
    grantResults: Array[Int]) {
    if (requestCode == MY_PERMISSIONS_REQUEST_CAMERA) {
      // If request is cancelled, the result arrays are empty.
      if (grantResults.length > 0
        && grantResults(0) == PackageManager.PERMISSION_GRANTED) {
          scannerView.setResultHandler(this)
          scannerView.startCamera()
      } else {
        Toast.makeText(this, R.string.add_profile_scanner_permission_required, Toast.LENGTH_SHORT).show()
        finish()
      }
    }
  }

  override def onCreate(state: Bundle) {
    super.onCreate(state)
    setContentView(R.layout.layout_scanner)
    setupToolbar()

    scannerView = new ZXingScannerView(this)
    val contentFrame = findViewById(R.id.content_frame).asInstanceOf[ViewGroup]
    contentFrame.addView(scannerView)
  }

  override def onResume() {
    super.onResume()
    val permissionCheck = ContextCompat.checkSelfPermission(this,
      Manifest.permission.CAMERA)
    if (permissionCheck == PackageManager.PERMISSION_GRANTED) {
      scannerView.setResultHandler(this) // Register ourselves as a handler for scan results.
      scannerView.startCamera()          // Start camera on resume
    } else {
      ActivityCompat.requestPermissions(this,
        Array(Manifest.permission.CAMERA), MY_PERMISSIONS_REQUEST_CAMERA)
    }
  }

  override def onPause() {
    super.onPause()
    scannerView.stopCamera()           // Stop camera on pause
  }

  override def handleResult(rawResult: Result) = {
    val intent = new Intent()
    intent.putExtra("uri", rawResult.getText)
    setResult(Activity.RESULT_OK, intent)
    finish()
  }

  def setupToolbar() {
    val toolbar = findViewById(R.id.toolbar).asInstanceOf[Toolbar]
    setSupportActionBar(toolbar)
    val ab = getSupportActionBar()
    if (ab != null) {
      ab.setDisplayHomeAsUpEnabled(true)
    }
  }

  override def onOptionsItemSelected(item: MenuItem): Boolean = {
    item.getItemId() match {
      // Respond to the action bar's Up/Home button
      case android.R.id.home =>
        setResult(Activity.RESULT_CANCELED, new Intent())
        finish()
        return true
      case _ => // Do nothing
    }
    return super.onOptionsItemSelected(item)
  }

}

