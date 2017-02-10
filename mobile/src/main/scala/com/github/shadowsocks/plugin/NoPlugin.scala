package com.github.shadowsocks.plugin

import com.github.shadowsocks.ShadowsocksApplication.app

/**
  * @author Mygod
  */
object NoPlugin extends Plugin {
  override def id: String = ""
  override def label: CharSequence = app.getText(com.github.shadowsocks.R.string.plugin_disabled)
}
