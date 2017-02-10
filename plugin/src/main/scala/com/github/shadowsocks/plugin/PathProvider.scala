package com.github.shadowsocks.plugin

import java.io.File

import android.database.MatrixCursor
import android.net.Uri

/**
  * Helper class to provide relative paths of files to copy.
  *
  * @author Mygod
  */
final class PathProvider private[plugin](baseUri: Uri, cursor: MatrixCursor) {
  private val basePath = baseUri.getPath match {
    case null => ""
    case p => p.stripPrefix("/").stripSuffix("/")
  }

  def addPath(path: String, mode: String = "644"): PathProvider = {
    val stripped = path.stripPrefix("/").stripSuffix("/")
    if (stripped.startsWith(basePath)) cursor.newRow()
      .add(PluginContract.COLUMN_PATH, stripped)
      .add(PluginContract.COLUMN_MODE, mode)
    this
  }
  def addTo(file: File, to: String = "", mode: String = "644"): PathProvider = {
    var sub = to + file.getName
    if (basePath.startsWith(sub)) if (file.isDirectory) {
      sub += '/'
      file.listFiles().foreach(addTo(_, sub, mode))
    } else addPath(sub, mode)
    this
  }
  def addAt(file: File, at: String = "", mode: String = "644"): PathProvider = {
    if (basePath.startsWith(at))
      if (file.isDirectory) file.listFiles().foreach(addTo(_, at, mode)) else addPath(at, mode)
    this
  }
}
