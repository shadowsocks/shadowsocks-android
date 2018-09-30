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

import android.database.sqlite.SQLiteCantOpenDatabaseException
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.utils.DirectBoot
import com.github.shadowsocks.utils.printLog
import java.io.IOException
import java.sql.SQLException

/**
 * SQLExceptions are not caught (and therefore will cause crash) for insert/update transactions
 * to ensure we are in a consistent state.
 */
object ProfileManager {
    interface Listener {
        fun onAdd(profile: Profile)
        fun onRemove(profileId: Long)
    }
    var listener: Listener? = null

    @Throws(SQLException::class)
    fun createProfile(profile: Profile = Profile()): Profile {
        profile.id = 0
        profile.userOrder = PrivateDatabase.profileDao.nextOrder() ?: 0
        profile.id = PrivateDatabase.profileDao.create(profile)
        listener?.onAdd(profile)
        return profile
    }

    /**
     * Note: It's caller's responsibility to update DirectBoot profile if necessary.
     */
    @Throws(SQLException::class)
    fun updateProfile(profile: Profile) = check(PrivateDatabase.profileDao.update(profile) == 1)

    @Throws(IOException::class)
    fun getProfile(id: Long): Profile? = try {
        PrivateDatabase.profileDao[id]
    } catch (ex: SQLiteCantOpenDatabaseException) {
        throw IOException(ex)
    } catch (ex: SQLException) {
        printLog(ex)
        null
    }

    @Throws(SQLException::class)
    fun delProfile(id: Long) {
        check(PrivateDatabase.profileDao.delete(id) == 1)
        listener?.onRemove(id)
        if (id == DataStore.profileId && DataStore.directBootAware) DirectBoot.clean()
    }

    @Throws(SQLException::class)
    fun clear() = PrivateDatabase.profileDao.deleteAll().also {
        // listener is not called since this won't be used in mobile submodule
        DirectBoot.clean()
    }

    @Throws(IOException::class)
    fun ensureNotEmpty() {
        val nonEmpty = try {
            PrivateDatabase.profileDao.isNotEmpty()
        } catch (ex: SQLiteCantOpenDatabaseException) {
            throw IOException(ex)
        } catch (ex: SQLException) {
            printLog(ex)
            false
        }
        if (!nonEmpty) DataStore.profileId = createProfile().id
    }

    @Throws(IOException::class)
    fun getAllProfiles(): List<Profile>? = try {
        PrivateDatabase.profileDao.list()
    } catch (ex: SQLiteCantOpenDatabaseException) {
        throw IOException(ex)
    } catch (ex: SQLException) {
        printLog(ex)
        null
    }
}
