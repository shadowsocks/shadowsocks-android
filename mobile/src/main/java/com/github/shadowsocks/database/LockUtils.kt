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

import android.database.sqlite.SQLiteDatabaseLockedException
import com.j256.ormlite.dao.Dao
import com.j256.ormlite.support.ConnectionSource
import com.j256.ormlite.table.TableUtils
import java.sql.SQLException

private val Throwable.ultimateCause: Throwable get() {
    var result = this
    while (true) {
        val cause = result.cause ?: return result
        result = cause
    }
}

@Throws(SQLException::class)
fun <T> safeWrapper(func: () -> T): T {
    while (true) {
        try {
            return func()
        } catch (e: SQLException) {
            if (e.ultimateCause !is SQLiteDatabaseLockedException) throw e
        }
    }
}

@Throws(SQLException::class)
inline fun <reified T> ConnectionSource.createTableSafe() = safeWrapper { TableUtils.createTable(this, T::class.java) }
@Throws(SQLException::class)
fun <T, ID> Dao<T, ID>.queryAllSafe(): MutableList<T> = safeWrapper { queryForAll() }
@Throws(SQLException::class)
fun <T, ID> Dao<T, ID>.queryByIdSafe(id: ID?): T? = safeWrapper { queryForId(id) }
@Throws(SQLException::class)
fun <T, ID> Dao<T, ID>.updateSafe(data: T?): Int = safeWrapper { update(data) }
@Throws(SQLException::class)
fun <T, ID> Dao<T, ID>.replaceSafe(data: T?): Dao.CreateOrUpdateStatus? = safeWrapper { createOrUpdate(data) }
@Throws(SQLException::class)
fun <T, ID> Dao<T, ID>.deleteByIdSafe(id: ID?) = safeWrapper { deleteById(id) }
@Throws(SQLException::class)
fun <T, ID> Dao<T, ID>.executeSafe(statement: String?) = safeWrapper { executeRawNoArgs(statement) }
