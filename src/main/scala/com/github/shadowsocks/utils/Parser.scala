package com.github.shadowsocks.utils

import com.github.shadowsocks.database.Profile
import android.net.Uri
import android.util.{Log, Base64}

object Parser {
  val TAG = "ShadowParser"
  def parse (data: String): Option[Profile] = {
    try {
      Log.d(TAG, data)
      val uri = Uri.parse(data.trim)
      if (uri.getScheme == Scheme.SS) {
        val encoded = data.replace(Scheme.SS + "://", "")
        val content = new String(Base64.decode(encoded, Base64.NO_PADDING), "UTF-8")
        val info = content.split('@')
        val encinfo = info(0).split(':')
        val serverinfo = info(1).split(':')

        val profile = new Profile
        profile.name = serverinfo(0)
        profile.host = serverinfo(0)
        profile.remotePort = serverinfo(1).toInt
        profile.localPort = 1080
        profile.method = encinfo(0)
        profile.password = encinfo(1)
        return Some(profile)
      }
    } catch {
      case ex : Exception => Log.e(TAG, "parser error", ex)// Ignore
    }
    None
  }
}
