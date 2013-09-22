package com.github.shadowsocks.database

import com.j256.ormlite.android.apptools.OrmLiteSqliteOpenHelper
import android.content.Context
import android.database.sqlite.SQLiteDatabase
import com.j256.ormlite.support.ConnectionSource
import com.j256.ormlite.table.TableUtils
import com.j256.ormlite.dao.Dao

class DBHelper(implicit val context: Context) extends OrmLiteSqliteOpenHelper(context, "postnauka", null, 3) {

  lazy val profileDao:Dao[Profile,Int] = getDao(classOf[Profile])

  def onCreate(database: SQLiteDatabase, connectionSource: ConnectionSource) {
    TableUtils.createTable(connectionSource, classOf[Profile])
  }

  def onUpgrade(database: SQLiteDatabase, connectionSource: ConnectionSource, oldVersion: Int, newVersion: Int) {
    TableUtils.dropTable(connectionSource, classOf[Profile], true)
    onCreate(database, connectionSource)
  }
}
