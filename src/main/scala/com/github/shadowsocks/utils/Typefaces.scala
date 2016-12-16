package com.github.shadowsocks.utils

import java.util

import android.content.Context
import android.graphics.Typeface
import android.util.Log
import com.github.shadowsocks.ShadowsocksApplication.app

object Typefaces {
  def get(c: Context, assetPath: String): Typeface = {
    cache synchronized {
      if (!cache.containsKey(assetPath)) {
        try {
          cache.put(assetPath, Typeface.createFromAsset(c.getAssets, assetPath))
        } catch {
          case e: Exception =>
            Log.e("Typefaces", "Could not get typeface '" + assetPath + "' because " + e.getMessage)
            app.track(e)
            return null
        }
      }
      return cache.get(assetPath)
    }
  }

  private final val cache = new util.Hashtable[String, Typeface]
}
