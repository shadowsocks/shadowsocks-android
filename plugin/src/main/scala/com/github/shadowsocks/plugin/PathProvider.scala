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

  def addPath(path: String): PathProvider = {
    val stripped = path.stripPrefix("/").stripSuffix("/")
    if (stripped.startsWith(basePath)) cursor.newRow().add(PluginInterface.COLUMN_PATH, stripped)
    this
  }
  def addTo(file: File, to: String = ""): PathProvider = {
    var sub = to + file.getName
    if (basePath.startsWith(sub)) if (file.isDirectory) {
      sub += '/'
      file.listFiles().foreach(addTo(_, sub))
    } else addPath(sub)
    this
  }
  def addAt(file: File, at: String = ""): PathProvider = {
    if (basePath.startsWith(at)) if (file.isDirectory) file.listFiles().foreach(addTo(_, at)) else addPath(at)
    this
  }
}
