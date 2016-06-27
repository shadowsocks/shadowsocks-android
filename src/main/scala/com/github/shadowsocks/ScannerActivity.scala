package com.github.shadowsocks

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import android.os.Handler
import android.support.v4.app.NavUtils
import android.support.v7.app.ActionBar
import android.support.v7.app.AppCompatActivity
import android.support.v7.widget.Toolbar
import android.util.AttributeSet
import android.util.TypedValue
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup

import com.google.zxing.Result

import me.dm7.barcodescanner.core.IViewFinder
import me.dm7.barcodescanner.core.ViewFinderView
import me.dm7.barcodescanner.zxing.ZXingScannerView

class ScannerActivity extends AppCompatActivity with ZXingScannerView.ResultHandler {
    var scannerView: ZXingScannerView = null

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
        scannerView.setResultHandler(this) // Register ourselves as a handler for scan results.
        scannerView.startCamera()          // Start camera on resume
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
        }
        return super.onOptionsItemSelected(item)
    }
}

