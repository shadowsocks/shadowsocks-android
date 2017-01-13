package com.github.shadowsocks.plugin

import android.content.pm.{PackageManager, ResolveInfo}
import android.graphics.drawable.Drawable
import com.github.shadowsocks.ShadowsocksApplication.app

/**
  * Base class for any kind of plugin that can be used.
  *
  * @author Mygod
  */
abstract class ResolvedPlugin(resolveInfo: ResolveInfo, packageManager: PackageManager = app.getPackageManager)
  extends Plugin {
  override final lazy val label: CharSequence = resolveInfo.loadLabel(packageManager)
  override final lazy val icon: Drawable = resolveInfo.loadIcon(packageManager)
}
