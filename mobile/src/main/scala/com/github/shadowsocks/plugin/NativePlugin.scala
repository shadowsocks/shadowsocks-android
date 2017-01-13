package com.github.shadowsocks.plugin

import android.content.pm.{PackageManager, ResolveInfo}

/**
  * @author Mygod
  */
class NativePlugin(resolveInfo: ResolveInfo, packageManager: PackageManager)
  extends ResolvedPlugin(resolveInfo, packageManager) {
  assert(resolveInfo.providerInfo != null)
  assert(resolveInfo.providerInfo.authority.startsWith(PluginInterface.AUTHORITY_BASE))
  override final lazy val id: String =
    resolveInfo.providerInfo.authority.substring(PluginInterface.AUTHORITY_BASE.length)
  override final lazy val defaultConfig: String =
    resolveInfo.providerInfo.metaData.getString(PluginInterface.METADATA_KEY_DEFAULT_CONFIG)
}
