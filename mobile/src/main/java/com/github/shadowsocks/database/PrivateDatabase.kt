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
import android.support.v7.preference.PreferenceManager
import android.text.TextUtils
import com.github.shadowsocks.App.Companion.app
import com.github.shadowsocks.utils.Key
import com.j256.ormlite.android.AndroidDatabaseConnection
import com.j256.ormlite.android.apptools.OrmLiteSqliteOpenHelper
import com.j256.ormlite.dao.Dao
import com.j256.ormlite.support.ConnectionSource
import com.j256.ormlite.table.TableUtils

object PrivateDatabase : OrmLiteSqliteOpenHelper(app, Key.DB_PROFILE, null, 25) {
    @Suppress("UNCHECKED_CAST")
    val profileDao: Dao<Profile, Int> by lazy { getDao(Profile::class.java) as Dao<Profile, Int> }
    @Suppress("UNCHECKED_CAST")
    val kvPairDao: Dao<KeyValuePair, String?> by lazy { getDao(KeyValuePair::class.java) as Dao<KeyValuePair, String?> }

    override fun onCreate(database: SQLiteDatabase?, connectionSource: ConnectionSource?) {
        TableUtils.createTable(connectionSource, Profile::class.java)
        TableUtils.createTable(connectionSource, KeyValuePair::class.java)
    }

    private fun recreate(database: SQLiteDatabase?, connectionSource: ConnectionSource?) {
        TableUtils.dropTable<Profile, Int>(connectionSource, Profile::class.java, true)
        TableUtils.dropTable<KeyValuePair, String?>(connectionSource, KeyValuePair::class.java, true)
        onCreate(database, connectionSource)
    }

    override fun onUpgrade(database: SQLiteDatabase?, connectionSource: ConnectionSource?,
                           oldVersion: Int, newVersion: Int) {
        if (oldVersion < 7) {
            recreate(database, connectionSource)
            return
        }

        try {
            if (oldVersion < 8) {
                profileDao.executeRawNoArgs("ALTER TABLE `profile` ADD COLUMN udpdns SMALLINT;")
            }
            if (oldVersion < 9) {
                profileDao.executeRawNoArgs("ALTER TABLE `profile` ADD COLUMN route VARCHAR DEFAULT 'all';")
            } else if (oldVersion < 19) {
                profileDao.executeRawNoArgs("UPDATE `profile` SET route = 'all' WHERE route IS NULL;")
            }
            if (oldVersion < 11) {
                profileDao.executeRawNoArgs("ALTER TABLE `profile` ADD COLUMN ipv6 SMALLINT;")
            }
            if (oldVersion < 12) {
                profileDao.executeRawNoArgs("BEGIN TRANSACTION;")
                profileDao.executeRawNoArgs("ALTER TABLE `profile` RENAME TO `tmp`;")
                TableUtils.createTable(connectionSource, Profile::class.java)
                profileDao.executeRawNoArgs(
                        "INSERT INTO `profile`(id, name, host, localPort, remotePort, password, method, route, proxyApps, bypass," +
                                " udpdns, ipv6, individual) " +
                                "SELECT id, name, host, localPort, remotePort, password, method, route, 1 - global, bypass, udpdns, ipv6," +
                                " individual FROM `tmp`;")
                profileDao.executeRawNoArgs("DROP TABLE `tmp`;")
                profileDao.executeRawNoArgs("COMMIT;")
            } else if (oldVersion < 13) {
                profileDao.executeRawNoArgs("ALTER TABLE `profile` ADD COLUMN tx LONG;")
                profileDao.executeRawNoArgs("ALTER TABLE `profile` ADD COLUMN rx LONG;")
                profileDao.executeRawNoArgs("ALTER TABLE `profile` ADD COLUMN date VARCHAR;")
            }

            if (oldVersion < 15) {
                if (oldVersion >= 12) profileDao.executeRawNoArgs("ALTER TABLE `profile` ADD COLUMN userOrder LONG;")
                var i = 0L
                val apps by lazy { app.packageManager.getInstalledApplications(0) }
                for (profile in profileDao.queryForAll()) {
                    if (oldVersion < 14) {
                        val uidSet = profile.individual.split('|').filter(TextUtils::isDigitsOnly)
                                .map(String::toInt).toSet()
                        profile.individual = apps.filter { uidSet.contains(it.uid) }
                                .joinToString("\n") { it.packageName }
                    }
                    profile.userOrder = i
                    profileDao.update(profile)
                    i += 1
                }
            }

            if (oldVersion < 16) {
                profileDao.executeRawNoArgs(
                        "UPDATE `profile` SET route = 'bypass-lan-china' WHERE route = 'bypass-china'")
            }

            if (oldVersion < 21) {
                profileDao.executeRawNoArgs("ALTER TABLE `profile` ADD COLUMN remoteDns VARCHAR DEFAULT '8.8.8.8';")
            }

            if (oldVersion < 17) {
                profileDao.executeRawNoArgs("ALTER TABLE `profile` ADD COLUMN plugin VARCHAR;")
            } else if (oldVersion < 22) {
                // upgrade kcptun to SIP003 plugin
                profileDao.executeRawNoArgs("BEGIN TRANSACTION;")
                profileDao.executeRawNoArgs("ALTER TABLE `profile` RENAME TO `tmp`;")
                TableUtils.createTable(connectionSource, Profile::class.java)
                profileDao.executeRawNoArgs(
                        "INSERT INTO `profile`(id, name, host, localPort, remotePort, password, method, route, " +
                                "remoteDns, proxyApps, bypass, udpdns, ipv6, individual, tx, rx, date, userOrder, " +
                                "plugin) " +
                                "SELECT id, name, host, localPort, " +
                                "CASE WHEN kcp = 1 THEN kcpPort ELSE remotePort END, password, method, route, " +
                                "remoteDns, proxyApps, bypass, udpdns, ipv6, individual, tx, rx, date, userOrder, " +
                                "CASE WHEN kcp = 1 THEN 'kcptun ' || kcpcli ELSE NULL END FROM `tmp`;")
                profileDao.executeRawNoArgs("DROP TABLE `tmp`;")
                profileDao.executeRawNoArgs("COMMIT;")
            }

            if (oldVersion < 23) {
                profileDao.executeRawNoArgs("BEGIN TRANSACTION;")
                TableUtils.createTable(connectionSource, KeyValuePair::class.java)
                profileDao.executeRawNoArgs("COMMIT;")
                val old = PreferenceManager.getDefaultSharedPreferences(app)
                PublicDatabase.kvPairDao.createOrUpdate(KeyValuePair(Key.id).put(old.getInt(Key.id, 0)))
                PublicDatabase.kvPairDao.createOrUpdate(KeyValuePair(Key.tfo).put(old.getBoolean(Key.tfo, false)))
            }

            if (oldVersion < 25) {
                PublicDatabase.onUpgrade(database, 0, -1)
            }
        } catch (ex: Exception) {
            app.track(ex)
            recreate(database, connectionSource)
        }
    }

    override fun onDowngrade(db: SQLiteDatabase?, oldVersion: Int, newVersion: Int) {
        val connection = AndroidDatabaseConnection(db, true)
        connectionSource.saveSpecialConnection(connection)
        recreate(db, connectionSource)
        connectionSource.clearSpecialConnection(connection)
    }
}
