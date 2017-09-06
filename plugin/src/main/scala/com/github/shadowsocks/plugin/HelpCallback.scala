package com.github.shadowsocks.plugin

import android.app.Activity
import android.content.Intent

/**
  * HelpCallback is an HelpActivity but you just need to produce a CharSequence help message instead of having to
  * provide UI. To create a help callback, just extend this class, implement abstract methods, and add it to your
  * manifest following the same procedure as adding a HelpActivity.
  *
  * @author Mygod
  */
trait HelpCallback extends HelpActivity {
  def produceHelpMessage(options: PluginOptions): CharSequence

  override protected def onInitializePluginOptions(options: PluginOptions) {
    setResult(Activity.RESULT_OK, new Intent().putExtra(PluginContract.EXTRA_HELP_MESSAGE, produceHelpMessage(options)))
    finish()
  }
}
