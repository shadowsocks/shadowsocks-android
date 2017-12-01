package com.github.shadowsocks.preference

import java.io.ByteArrayOutputStream
import java.nio.ByteBuffer
import java.util

import android.os.Binder
import android.support.v7.preference.PreferenceDataStore
import com.github.shadowsocks.database.{DBHelper, KeyValuePair}
import com.github.shadowsocks.utils.{Key, Utils}

import scala.collection.JavaConversions._

/**
  * @author Mygod
  */
//noinspection AccessorLikeMethodIsUnit
final class OrmLitePreferenceDataStore(dbHelper: DBHelper) extends PreferenceDataStore {
  import KeyValuePair._

  override def getBoolean(key: String, defValue: Boolean = false): Boolean = dbHelper.kvPairDao.queryForId(key) match {
    case pair: KeyValuePair =>
      if (pair.valueType == TYPE_BOOLEAN) ByteBuffer.wrap(pair.value).get() != 0 else defValue
    case _ => defValue
  }
  override def getFloat(key: String, defValue: Float): Float = dbHelper.kvPairDao.queryForId(key) match {
    case pair: KeyValuePair =>
      if (pair.valueType == TYPE_FLOAT) ByteBuffer.wrap(pair.value).getFloat() else defValue
    case _ => defValue
  }
  override def getInt(key: String, defValue: Int): Int = dbHelper.kvPairDao.queryForId(key) match {
    case pair: KeyValuePair =>
      if (pair.valueType == TYPE_INT) ByteBuffer.wrap(pair.value).getInt() else defValue
    case _ => defValue
  }
  override def getLong(key: String, defValue: Long): Long = dbHelper.kvPairDao.queryForId(key) match {
    case pair: KeyValuePair =>
      if (pair.valueType == TYPE_LONG) ByteBuffer.wrap(pair.value).getLong() else defValue
    case _ => defValue
  }
  override def getString(key: String, defValue: String = null): String = dbHelper.kvPairDao.queryForId(key) match {
    case pair: KeyValuePair =>
      if (pair.valueType == TYPE_STRING) new String(pair.value) else defValue
    case _ => defValue
  }
  override def getStringSet(key: String, defValue: util.Set[String]): util.Set[String] =
    dbHelper.kvPairDao.queryForId(key) match {
      case pair: KeyValuePair => if (pair.valueType == TYPE_STRING_SET) {
        val buffer = ByteBuffer.wrap(pair.value)
        val result = new util.HashSet[String]()
        while (buffer.hasRemaining) {
          val chArr = new Array[Byte](buffer.getInt)
          buffer.get(chArr)
          result.add(new String(chArr))
        }
        result
      } else defValue
      case _ => defValue
    }

  override def putBoolean(key: String, value: Boolean) {
    dbHelper.kvPairDao.createOrUpdate(
      new KeyValuePair(key, TYPE_BOOLEAN, ByteBuffer.allocate(1).put((if (value) 1 else 0).toByte).array()))
    fireChangeListener(key)
  }
  override def putFloat(key: String, value: Float) {
    dbHelper.kvPairDao.createOrUpdate(new KeyValuePair(key, TYPE_FLOAT, ByteBuffer.allocate(4).putFloat(value).array()))
    fireChangeListener(key)
  }
  override def putInt(key: String, value: Int) {
    dbHelper.kvPairDao.createOrUpdate(new KeyValuePair(key, TYPE_INT, ByteBuffer.allocate(4).putInt(value).array()))
    fireChangeListener(key)
  }
  override def putLong(key: String, value: Long) {
    dbHelper.kvPairDao.createOrUpdate(new KeyValuePair(key, TYPE_LONG, ByteBuffer.allocate(8).putLong(value).array()))
    fireChangeListener(key)
  }
  override def putString(key: String, value: String) {
    value match {
      case null => remove(key)
      case _ => dbHelper.kvPairDao.createOrUpdate(new KeyValuePair(key, TYPE_STRING, value.getBytes()))
    }
    fireChangeListener(key)
  }
  override def putStringSet(key: String, value: util.Set[String]) {
    val stream = new ByteArrayOutputStream()
    for (v <- value) {
      stream.write(ByteBuffer.allocate(4).putInt(v.length).array())
      stream.write(v.getBytes())
    }
    dbHelper.kvPairDao.createOrUpdate(new KeyValuePair(key, TYPE_STRING_SET, stream.toByteArray))
    fireChangeListener(key)
  }

  def remove(key: String): Int = dbHelper.kvPairDao.deleteById(key)
  
  private var listeners: Set[OnPreferenceDataStoreChangeListener] = Set.empty
  private def fireChangeListener(key: String) = listeners.foreach(_.onPreferenceDataStoreChanged(this, key))
  def registerChangeListener(listener: OnPreferenceDataStoreChangeListener): Unit = listeners += listener
  def unregisterChangeListener(listener: OnPreferenceDataStoreChangeListener): Unit = listeners -= listener

  // hopefully hashCode = mHandle doesn't change, currently this is true from KitKat to Nougat
  private lazy val userIndex = Binder.getCallingUserHandle.hashCode
  private def getLocalPort(key: String, default: Int) = getInt(key, 0) match {
    case 0 => Utils.parsePort(getString(key), default + userIndex)
    case value =>
      putString(key, value.toString)
      value
  }

  def profileId: Int = getInt(Key.id, 0)
  def profileId_=(i: Int): Unit = putInt(Key.id, i)
  def serviceMode: String = getString(Key.serviceMode, Key.modeVpn)
  def portProxy: Int = getLocalPort(Key.portProxy, 1080)
  def portLocalDns: Int = getLocalPort(Key.portLocalDns, 5450)
  def portTransproxy: Int = getLocalPort(Key.portTransproxy, 8200)

  def proxyApps: Boolean = getBoolean(Key.proxyApps)
  def proxyApps_=(value: Boolean): Unit = putBoolean(Key.proxyApps, value)
  def bypass: Boolean = getBoolean(Key.bypass)
  def bypass_=(value: Boolean): Unit = putBoolean(Key.bypass, value)
  def individual: String = getString(Key.individual)
  def individual_=(value: String): Unit = putString(Key.individual, value)
  def plugin: String = getString(Key.plugin)
  def plugin_=(value: String): Unit = putString(Key.plugin, value)
  def dirty: Boolean = getBoolean(Key.dirty)
  def dirty_=(value: Boolean): Unit = putBoolean(Key.dirty, value)

  def initGlobal() {
    // temporary workaround for support lib bug
    if (getString(Key.serviceMode) == null) putString(Key.serviceMode, serviceMode)
    if (getString(Key.portProxy) == null) putString(Key.portProxy, portProxy.toString)
    if (getString(Key.portLocalDns) == null) putString(Key.portLocalDns, portLocalDns.toString)
    if (getString(Key.portTransproxy) == null) putString(Key.portTransproxy, portTransproxy.toString)
  }
}
