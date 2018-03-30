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
import android.util.Log
import com.github.shadowsocks.App.Companion.app
import com.github.shadowsocks.ProfilesFragment
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.utils.DirectBoot
import java.io.IOException
import java.sql.SQLException

/**
 * SQLExceptions are not caught (and therefore will cause crash) for insert/update transactions
 * to ensure we are in a consistent state.
 */
object ProfileManager {
    private const val TAG = "ProfileManager"

    @Throws(SQLException::class)
    fun createProfile(p: Profile? = null): Profile {
        val profile = p ?: Profile()
        profile.id = 0
        val oldProfile = app.currentProfile
        if (oldProfile != null) {
            // Copy Feature Settings from old profile
            profile.route = oldProfile.route
            profile.ipv6 = oldProfile.ipv6
            profile.proxyApps = oldProfile.proxyApps
            profile.bypass = oldProfile.bypass
            profile.individual = oldProfile.individual
            profile.udpdns = oldProfile.udpdns
        }
        val last = PrivateDatabase.profileDao.queryRaw(PrivateDatabase.profileDao.queryBuilder()
                .selectRaw("MAX(userOrder)").prepareStatementString()).firstResult
        if (last != null && last.size == 1 && last[0] != null) profile.userOrder = last[0].toLong() + 1
        PrivateDatabase.profileDao.createOrUpdate(profile)
        ProfilesFragment.instance?.profilesAdapter?.add(profile)
        return profile
    }

    /**
     * Note: It's caller's responsibility to update DirectBoot profile if necessary.
     */
    @Throws(SQLException::class)
    fun updateProfile(profile: Profile) = PrivateDatabase.profileDao.update(profile)

    @Throws(IOException::class)
    fun getProfile(id: Int): Profile? = try {
        PrivateDatabase.profileDao.queryForId(id)
    } catch (ex: SQLException) {
        if (ex.cause is SQLiteCantOpenDatabaseException) throw IOException(ex)
        Log.e(TAG, "getProfile", ex)
        app.track(ex)
        null
    }

    @Throws(SQLException::class)
    fun delProfile(id: Int) {
        PrivateDatabase.profileDao.deleteById(id)
        ProfilesFragment.instance?.profilesAdapter?.removeId(id)
        if (id == DataStore.profileId && DataStore.directBootAware) DirectBoot.clean()
    }

    @Throws(IOException::class)
    fun getFirstProfile(): Profile? = try {
        PrivateDatabase.profileDao.query(PrivateDatabase.profileDao.queryBuilder().limit(1L).prepare()).singleOrNull()
    } catch (ex: SQLException) {
        if (ex.cause is SQLiteCantOpenDatabaseException) throw IOException(ex)
        Log.e(TAG, "getFirstProfile", ex)
        app.track(ex)
        null
    }

    @Throws(IOException::class)
    fun getAllProfiles(): List<Profile>? = try {
        PrivateDatabase.profileDao.query(PrivateDatabase.profileDao.queryBuilder().orderBy("userOrder", true).prepare())
    } catch (ex: SQLException) {
        if (ex.cause is SQLiteCantOpenDatabaseException) throw IOException(ex)
        Log.e(TAG, "getAllProfiles", ex)
        app.track(ex)
        null
    }
}
