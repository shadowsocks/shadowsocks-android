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
import android.util.LongSparseArray
import com.github.shadowsocks.Core
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.utils.DirectBoot
import com.github.shadowsocks.utils.forEachTry
import com.github.shadowsocks.utils.printLog
import com.google.gson.JsonStreamParser
import org.json.JSONArray
import java.io.IOException
import java.io.InputStream
import java.sql.SQLException

/**
 * SQLExceptions are not caught (and therefore will cause crash) for insert/update transactions
 * to ensure we are in a consistent state.
 */
object ProfileManager {
    interface Listener {
        fun onAdd(profile: Profile)
        fun onRemove(profileId: Long)
        fun onCleared()
        fun reloadProfiles()
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

    fun createProfilesFromJson(jsons: Sequence<InputStream>, replace: Boolean = false) {
        val profiles = if (replace) getAllProfiles()?.associateBy { it.formattedAddress } else null
        val feature = if (replace) {
            profiles?.values?.singleOrNull { it.id == DataStore.profileId }
        } else Core.currentProfile?.first
        val lazyClear = lazy { clear() }
        jsons.asIterable().forEachTry { json ->
            Profile.parseJson(JsonStreamParser(json.bufferedReader()).asSequence().single(), feature) {
                if (replace) {
                    lazyClear.value
                    // if two profiles has the same address, treat them as the same profile and copy stats over
                    profiles?.get(it.formattedAddress)?.apply {
                        it.tx = tx
                        it.rx = rx
                    }
                }
                createProfile(it)
            }
        }
    }

    fun serializeToJson(profiles: List<Profile>? = getActiveProfiles()): JSONArray? {
        if (profiles == null) return null
        val lookup = LongSparseArray<Profile>(profiles.size).apply { profiles.forEach { put(it.id, it) } }
        return JSONArray(profiles.map { it.toJson(lookup) }.toTypedArray())
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

    @Throws(IOException::class)
    fun expand(profile: Profile): Pair<Profile, Profile?> = Pair(profile, profile.udpFallback?.let { getProfile(it) })

    @Throws(SQLException::class)
    fun delProfile(id: Long) {
        check(PrivateDatabase.profileDao.delete(id) == 1)
        listener?.onRemove(id)
        if (id in Core.activeProfileIds && DataStore.directBootAware) DirectBoot.clean()
    }

    @Throws(SQLException::class)
    fun clear() = PrivateDatabase.profileDao.deleteAll().also {
        // listener is not called since this won't be used in mobile submodule
        DirectBoot.clean()
        listener?.onCleared()
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
    fun getActiveProfiles(): List<Profile>? = try {
        PrivateDatabase.profileDao.listActive()
    } catch (ex: SQLiteCantOpenDatabaseException) {
        throw IOException(ex)
    } catch (ex: SQLException) {
        printLog(ex)
        null
    }

    @Throws(IOException::class)
    fun getAllProfiles(): List<Profile>? = try {
        PrivateDatabase.profileDao.listAll()
    } catch (ex: SQLiteCantOpenDatabaseException) {
        throw IOException(ex)
    } catch (ex: SQLException) {
        printLog(ex)
        null
    }
}
