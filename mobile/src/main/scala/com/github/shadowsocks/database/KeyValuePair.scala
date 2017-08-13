package com.github.shadowsocks.database

import com.j256.ormlite.field.{DataType, DatabaseField}

/**
  * @author Mygod
  */
object KeyValuePair {
  val TYPE_UNINITIALIZED = 0
  val TYPE_BOOLEAN = 1
  val TYPE_FLOAT = 2
  val TYPE_INT = 3
  val TYPE_LONG = 4
  val TYPE_STRING = 5
  val TYPE_STRING_SET = 6
}

class KeyValuePair {
  @DatabaseField(id = true)
  var key: String = _
  @DatabaseField
  var valueType: Int = _
  @DatabaseField(dataType = DataType.BYTE_ARRAY)
  var value: Array[Byte] = _

  def this(key: String, valueType: Int, value: Array[Byte]) = {
    this()
    this.key = key
    this.valueType = valueType
    this.value = value
  }
}
