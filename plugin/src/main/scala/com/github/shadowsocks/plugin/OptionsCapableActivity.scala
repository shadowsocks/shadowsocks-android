package com.github.shadowsocks.plugin

import android.content.Intent
import android.os.Bundle
import android.support.v7.app.AppCompatActivity
import android.widget.Toast

/**
  * Activity that's capable of getting EXTRA_OPTIONS input.
  *
  * @author Mygod
  */
trait OptionsCapableActivity extends AppCompatActivity {
  protected def pluginOptions(intent: Intent = getIntent): PluginOptions =
    try new PluginOptions(intent.getStringExtra(PluginContract.EXTRA_OPTIONS)) catch {
      case exc: IllegalArgumentException =>
        Toast.makeText(this, exc.getMessage, Toast.LENGTH_SHORT).show()
        null
    }

  /**
    * Populate args to your user interface.
    *
    * @param options PluginOptions parsed.
    */
  protected def onInitializePluginOptions(options: PluginOptions = pluginOptions()): Unit

  override protected def onPostCreate(savedInstanceState: Bundle) {
    super.onPostCreate(savedInstanceState)
    if (savedInstanceState == null) onInitializePluginOptions()
  }
}
