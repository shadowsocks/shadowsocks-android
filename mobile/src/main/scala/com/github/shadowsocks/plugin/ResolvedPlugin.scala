package com.github.shadowsocks.plugin

import android.content.pm.{PackageManager, ResolveInfo}
import android.graphics.drawable.Drawable
import android.os.Bundle
import com.github.shadowsocks.ShadowsocksApplication.app

/**
  * Base class for any kind of plugin that can be used.
  *
  * @author Mygod
  */
abstract class ResolvedPlugin(resolveInfo: ResolveInfo, packageManager: PackageManager = app.getPackageManager)
  extends Plugin {
  protected def metaData: Bundle

  override final lazy val id: String = metaData.getString(PluginContract.METADATA_KEY_ID)
  override final lazy val label: CharSequence = resolveInfo.loadLabel(packageManager)
  override final lazy val icon: Drawable = resolveInfo.loadIcon(packageManager)
  override final lazy val defaultConfig: String = metaData.getString(PluginContract.METADATA_KEY_DEFAULT_CONFIG)
  override def packageName: String = resolveInfo.resolvePackageName
  override final lazy val trusted: Boolean = app.info.signatures.exists(PluginManager.trustedSignatures.contains)
}
