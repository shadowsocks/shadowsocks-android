package com.github.shadowsocks.database

import com.j256.ormlite.field.{DataType, DatabaseField}

class Profile {
  @DatabaseField(id = true)
  var id: Int = 0

  @DatabaseField
  var name: String = null

  @DatabaseField(foreign = true, canBeNull = false)
  var host: String = null

  @DatabaseField
  var localPort: Int = 0

  @DatabaseField
  var remotePort: Int = 0

  @DatabaseField
  var password: String = null

  @DatabaseField
  var method: String = null

  @DatabaseField
  var date: String = null

  @DatabaseField
  var upload: Int = 0

  @DatabaseField
  var download: Int = 0

  @DatabaseField
  var chnroute: Boolean = false

  @DatabaseField
  var global: Boolean = false

  @DatabaseField(dataType = DataType.LONG_STRING)
  var individual: String = null

  @DatabaseField(dataType = DataType.LONG_STRING)
  var description: String = null
}
