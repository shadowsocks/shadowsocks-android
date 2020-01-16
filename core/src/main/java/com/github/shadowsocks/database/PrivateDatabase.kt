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

import androidx.room.Database
import androidx.room.Room
import androidx.room.RoomDatabase
import androidx.room.TypeConverters
import androidx.room.migration.Migration
import androidx.sqlite.db.SupportSQLiteDatabase
import com.github.shadowsocks.Core.app
import com.github.shadowsocks.utils.Key
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch

@Database(entities = [Profile::class, KeyValuePair::class, SSRSub::class], version = 3)
@TypeConverters(Profile.SubscriptionStatus::class)
abstract class PrivateDatabase : RoomDatabase() {
    companion object {
        private val instance by lazy {
            Room.databaseBuilder(app, PrivateDatabase::class.java, Key.DB_PROFILE).apply {
                addMigrations(Migration2, Migration3)
                allowMainThreadQueries()
                enableMultiInstanceInvalidation()
                fallbackToDestructiveMigration()
                setQueryExecutor { GlobalScope.launch { it.run() } }
            }.build()
        }

        val profileDao get() = instance.profileDao()
        val kvPairDao get() = instance.keyValuePairDao()
        val ssrSubDao get() = instance.ssrSubDao()
    }
    abstract fun profileDao(): Profile.Dao
    abstract fun keyValuePairDao(): KeyValuePair.Dao
    abstract fun ssrSubDao(): SSRSub.Dao

    object Migration2 : Migration(1, 2) {
        override fun migrate(database: SupportSQLiteDatabase) =
                database.execSQL("ALTER TABLE `SSRSub` ADD COLUMN `status` INTEGER NOT NULL DEFAULT 0")
    }

    object Migration3 : Migration(2, 3) {
        override fun migrate(database: SupportSQLiteDatabase) =
                database.execSQL("ALTER TABLE `Profile` ADD COLUMN `subscription` INTEGER NOT NULL DEFAULT " +
                        Profile.SubscriptionStatus.UserConfigured.persistedValue)
    }
}
