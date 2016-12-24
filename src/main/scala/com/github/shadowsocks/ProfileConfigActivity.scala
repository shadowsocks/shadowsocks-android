package com.github.shadowsocks

import android.app.Activity
import android.content.DialogInterface
import android.os.Bundle
import android.support.v7.app.AlertDialog
import android.support.v7.widget.Toolbar
import com.github.shadowsocks.ShadowsocksApplication.app
import com.github.shadowsocks.utils.Key

class ProfileConfigActivity extends Activity {
  private lazy val child = getFragmentManager.findFragmentById(R.id.content).asInstanceOf[ProfileConfigFragment]

  override def onCreate(savedInstanceState: Bundle) {
    super.onCreate(savedInstanceState)
    setContentView(R.layout.layout_profile_config)
    val toolbar = findViewById(R.id.toolbar).asInstanceOf[Toolbar]
    toolbar.setTitle("Profile config")  // TODO
    toolbar.setNavigationIcon(R.drawable.ic_navigation_close)
    toolbar.setNavigationOnClickListener(_ => onBackPressed())
    toolbar.inflateMenu(R.menu.profile_config_menu)
    toolbar.setOnMenuItemClickListener(child)
  }

  override def onBackPressed(): Unit = if (app.settings.getBoolean(Key.dirty, false)) new AlertDialog.Builder(this)
    .setTitle("Changes not saved. Do you want to save?") // TODO: localizations
    .setPositiveButton("Yes", ((_, _) => child.saveAndExit()): DialogInterface.OnClickListener)
    .setNegativeButton("No", ((_, _) => finish()): DialogInterface.OnClickListener)
    .setNeutralButton(android.R.string.cancel, null)
    .create()
    .show() else super.onBackPressed()
}
