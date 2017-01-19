package com.github.shadowsocks.plugin

import android.content.pm.{PackageManager, ProviderInfo}

/**
  * @author Mygod
  */
class NativePlugin(providerInfo: ProviderInfo, packageManager: PackageManager)
  extends ResolvedPlugin(providerInfo, packageManager) {
  assert(providerInfo != null)
  assert(providerInfo.authority.startsWith(PluginInterface.AUTHORITY_BASE))
  override final lazy val id: String =
    providerInfo.authority.substring(PluginInterface.AUTHORITY_BASE.length)
  override final lazy val defaultConfig: String =
    providerInfo.metaData.getString(PluginInterface.METADATA_KEY_DEFAULT_CONFIG)
}
