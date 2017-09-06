package com.github.shadowsocks.plugin

import android.content.pm.{PackageManager, ResolveInfo}
import android.os.Bundle

/**
  * @author Mygod
  */
final class NativePlugin(resolveInfo: ResolveInfo, packageManager: PackageManager)
  extends ResolvedPlugin(resolveInfo, packageManager) {
  assert(resolveInfo.providerInfo != null)
  override protected def metaData: Bundle = resolveInfo.providerInfo.metaData
  override def packageName: String = resolveInfo.providerInfo.packageName
}
