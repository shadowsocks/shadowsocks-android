package com.github.shadowsocks.plugin

import android.graphics.drawable.Drawable

/**
  * @author Mygod
  */
abstract class Plugin {
  def id: String
  def label: CharSequence
  def icon: Drawable = null
  def defaultConfig: String = null
  def packageName: String = null
  def trusted: Boolean = true
}
