package com.github.shadowsocks.plugin

import android.content.{ContentProvider, ContentValues}
import android.database.{Cursor, MatrixCursor}
import android.net.Uri
import android.os.ParcelFileDescriptor

/**
  * Base class for a native plugin provider. A native plugin provider offers read-only access to files that are required
  * to run a plugin, such as binary files and other configuration files. To create a native plugin provider, extend this
  * class, implement the abstract methods, and add it to your manifest like this:
  *
  * <pre class="prettyprint">&lt;manifest&gt;
  *    ...
  *    &lt;application&gt;
  *        ...
  *        &lt;provider android:name="com.github.shadowsocks.$PLUGIN_ID.PluginProvider"
  *                     android:authorities="com.github.shadowsocks.$PLUGIN_ID"&gt;
  *            &lt;intent-filter&gt;
  *                &lt;category android:name="com.github.shadowsocks.plugin.CATEGORY_NATIVE_PLUGIN" /&gt;
  *            &lt;/intent-filter&gt;
  *        &lt;/provider&gt;
  *        ...
  *    &lt;/application&gt;
  *&lt;/manifest&gt;</pre>
  *
  * @author Mygod
  */
abstract class NativePluginProvider extends ContentProvider {
  /**
    * @inheritdoc
    *
    * NativePluginProvider returns application/x-elf by default. It's implementer's responsibility to change this to
    * correct type.
    */
  override def getType(uri: Uri): String = "application/x-elf"

  override def onCreate(): Boolean = true

  /**
    * Provide all files needed for native plugin.
    *
    * @param provider A helper object to use to add files.
    */
  protected def populateFiles(provider: PathProvider)

  override def query(uri: Uri, projection: Array[String], selection: String, selectionArgs: Array[String],
                     sortOrder: String): Cursor = {
    if (selection != null || selectionArgs != null || sortOrder != null) ???
    val result = new MatrixCursor(projection.filter(_ == PluginInterface.COLUMN_PATH))
    populateFiles(new PathProvider(uri, result))
    result
  }

  def openFile(uri: Uri): ParcelFileDescriptor
  override def openFile(uri: Uri, mode: String): ParcelFileDescriptor = {
    if (mode != "r") ???
    openFile(uri)
  }

  // Methods that should not be used
  override def update(uri: Uri, values: ContentValues, selection: String, selectionArgs: Array[String]): Int = ???
  override def insert(uri: Uri, values: ContentValues): Uri = ???
  override def delete(uri: Uri, selection: String, selectionArgs: Array[String]): Int = ???
}
