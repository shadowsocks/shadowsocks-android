package com.github.shadowsocks

import android.app.{Activity, TaskStackBuilder}
import android.content.Intent
import android.content.pm.{PackageManager, ShortcutManager}
import android.os.{Build, Bundle}
import android.support.v4.app.ActivityCompat
import android.support.v4.content.ContextCompat
import android.support.v7.app.AppCompatActivity
import android.support.v7.widget.Toolbar
import android.text.TextUtils
import android.widget.Toast
import com.google.zxing.Result
import com.github.shadowsocks.ShadowsocksApplication.app
import com.github.shadowsocks.utils.Parser
import me.dm7.barcodescanner.zxing.ZXingScannerView

object ScannerActivity {
  private final val MY_PERMISSIONS_REQUEST_CAMERA = 1
}

class ScannerActivity extends AppCompatActivity with ZXingScannerView.ResultHandler {
  import ScannerActivity._

  private var scannerView: ZXingScannerView = _

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

  def navigateUp() {
    val intent = getParentActivityIntent
    if (shouldUpRecreateTask(intent) || isTaskRoot)
      TaskStackBuilder.create(this).addNextIntentWithParentStack(intent).startActivities()
    else finish()
  }

  override def onCreate(state: Bundle) {
    super.onCreate(state)
    setContentView(R.layout.layout_scanner)
    val toolbar = findViewById(R.id.toolbar).asInstanceOf[Toolbar]
    toolbar.setTitle(getTitle)
    toolbar.setNavigationIcon(R.drawable.abc_ic_ab_back_material)
    toolbar.setNavigationOnClickListener(_ => navigateUp())
    scannerView = findViewById(R.id.scanner).asInstanceOf[ZXingScannerView]
    if (Build.VERSION.SDK_INT >= 25) getSystemService(classOf[ShortcutManager]).reportShortcutUsed("scan")
  }

  override def onResume() {
    super.onResume()
    val permissionCheck = ContextCompat.checkSelfPermission(this,
      android.Manifest.permission.CAMERA)
    if (permissionCheck == PackageManager.PERMISSION_GRANTED) {
      scannerView.setResultHandler(this) // Register ourselves as a handler for scan results.
      scannerView.setAutoFocus(true)
      scannerView.startCamera()          // Start camera on resume
    } else {
      ActivityCompat.requestPermissions(this,
        Array(android.Manifest.permission.CAMERA), MY_PERMISSIONS_REQUEST_CAMERA)
    }
  }

  override def onPause() {
    super.onPause()
    scannerView.stopCamera()           // Stop camera on pause
  }

  override def handleResult(rawResult: Result) = {
    val uri = rawResult.getText
    if (!TextUtils.isEmpty(uri))
      Parser.findAll(uri).foreach(app.profileManager.createProfile)
      Parser.findAll_ssr(uri).foreach(app.profileManager.createProfile)
    navigateUp()
  }
}
