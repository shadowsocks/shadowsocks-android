package com.github.shadowsocks.plugin

import android.content.pm.{PackageManager, ProviderInfo}
import android.graphics.drawable.Drawable
import com.github.shadowsocks.ShadowsocksApplication.app

/**
  * Base class for any kind of plugin that can be used.
  *
  * @author Mygod
  */
abstract class ResolvedPlugin(providerInfo: ProviderInfo, packageManager: PackageManager = app.getPackageManager)
  extends Plugin {
  override final lazy val label: CharSequence = packageManager.getApplicationLabel(providerInfo.applicationInfo)
  override final lazy val icon: Drawable = packageManager.getApplicationIcon(providerInfo.applicationInfo)
}
