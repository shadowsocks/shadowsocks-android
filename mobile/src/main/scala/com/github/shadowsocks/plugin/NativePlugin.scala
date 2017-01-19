package com.github.shadowsocks.plugin

import android.content.ContentResolver
import android.content.pm.{PackageManager, ResolveInfo}
import android.net.Uri
import android.os.Bundle

/**
  * @author Mygod
  */
final class NativePlugin(resolveInfo: ResolveInfo, packageManager: PackageManager)
  extends ResolvedPlugin(resolveInfo, packageManager) {
  assert(resolveInfo.providerInfo != null)
  override protected def metaData: Bundle = resolveInfo.providerInfo.metaData

  private lazy val uriBuilder: Uri.Builder = new Uri.Builder()
    .scheme(ContentResolver.SCHEME_CONTENT)
    .authority(resolveInfo.providerInfo.authority)
}
