package com.github.shadowsocks

import android.app.Activity
import android.os.Bundle
import android.support.v7.widget.Toolbar
import android.support.v7.widget.Toolbar.OnMenuItemClickListener

class ProfileConfigActivity extends Activity {
  override def onCreate(savedInstanceState: Bundle) {
    super.onCreate(savedInstanceState)
    setContentView(R.layout.layout_profile_config)
    val toolbar = findViewById(R.id.toolbar).asInstanceOf[Toolbar]
    toolbar.setTitle("Profile config")  // TODO
    toolbar.setNavigationIcon(R.drawable.ic_navigation_close)
    toolbar.setNavigationOnClickListener(_ => finish())
    toolbar.inflateMenu(R.menu.profile_config_menu)
    toolbar.setOnMenuItemClickListener(getFragmentManager.findFragmentById(R.id.content)
      .asInstanceOf[OnMenuItemClickListener])
  }
}
