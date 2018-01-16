/*******************************************************************************
 *                                                                             *
 *  Copyright (C) 2017 by Max Lv <max.c.lv@gmail.com>                          *
 *  Copyright (C) 2017 by Mygod Studio <contact-shadowsocks-android@mygod.be>  *
 *                                                                             *
 *  This program is free software: you can redistribute it and/or modify       *
 *  it under the terms of the GNU General Public License as published by       *
 *  the Free Software Foundation, either version 3 of the License, or          *
 *  (at your option) any later version.                                        *
 *                                                                             *
 *  This program is distributed in the hope that it will be useful,            *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 *  GNU General Public License for more details.                               *
 *                                                                             *
 *  You should have received a copy of the GNU General Public License          *
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.       *
 *                                                                             *
 *******************************************************************************/

package com.github.shadowsocks.database

import android.database.sqlite.SQLiteDatabase
import com.github.shadowsocks.App.Companion.app
import com.github.shadowsocks.acl.Acl
import com.github.shadowsocks.utils.Key
import com.j256.ormlite.android.AndroidDatabaseConnection
import com.j256.ormlite.android.apptools.OrmLiteSqliteOpenHelper
import com.j256.ormlite.dao.Dao
import com.j256.ormlite.support.ConnectionSource
import com.j256.ormlite.table.TableUtils

object PublicDatabase : OrmLiteSqliteOpenHelper(app.deviceContext, Key.DB_PUBLIC, null, 2) {
    @Suppress("UNCHECKED_CAST")
    val kvPairDao: Dao<KeyValuePair, String?> by lazy { getDao(KeyValuePair::class.java) as Dao<KeyValuePair, String?> }

    override fun onCreate(database: SQLiteDatabase?, connectionSource: ConnectionSource?) {
        TableUtils.createTable(connectionSource, KeyValuePair::class.java)
    }

    override fun onUpgrade(database: SQLiteDatabase?, connectionSource: ConnectionSource?,
                           oldVersion: Int, newVersion: Int) {
        if (oldVersion < 1) {
            PrivateDatabase.kvPairDao.queryBuilder().where().`in`("key",
                    Key.id, Key.tfo, Key.serviceMode, Key.portProxy, Key.portLocalDns, Key.portTransproxy).query()
                    .forEach { kvPairDao.createOrUpdate(it) }
        }

        if (oldVersion < 2) {
            kvPairDao.createOrUpdate(KeyValuePair(Acl.CUSTOM_RULES).put(Acl().fromId(Acl.CUSTOM_RULES).toString()))
        }
    }

    override fun onDowngrade(db: SQLiteDatabase?, oldVersion: Int, newVersion: Int) {
        val connection = AndroidDatabaseConnection(db, true)
        connectionSource.saveSpecialConnection(connection)
        TableUtils.dropTable<KeyValuePair, String?>(connectionSource, KeyValuePair::class.java, true)
        onCreate(db, connectionSource)
        connectionSource.clearSpecialConnection(connection)
    }
}
