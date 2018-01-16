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

package com.github.shadowsocks.acl

import android.util.Log
import com.evernote.android.job.Job
import com.evernote.android.job.JobCreator
import com.evernote.android.job.JobRequest
import java.io.IOException
import java.net.URL
import java.util.concurrent.TimeUnit

class AclSyncJob(private val route: String) : Job() {
    companion object : JobCreator {
        private const val TAG = "AclSyncJob"
        private const val MAX_RESCHEDULE = 3

        override fun create(tag: String): AclSyncJob? {
            val parts = tag.split(':', limit = 2)
            return when (parts[0]) {
                TAG -> AclSyncJob(parts[1])
                else -> {
                    Log.w(TAG, "Unknown job tag: " + tag)
                    null
                }
            }
        }

        fun schedule(route: String): Int = JobRequest.Builder(TAG + ':' + route)
                .setExecutionWindow(TimeUnit.SECONDS.toMillis(10), TimeUnit.DAYS.toMillis(28))
                .setRequirementsEnforced(true)
                .setRequiredNetworkType(JobRequest.NetworkType.UNMETERED)
                .setRequiresCharging(true)
                .setUpdateCurrent(true)
                .build().schedule()
    }

    override fun onRunJob(params: Params): Result = try {
        val acl = URL("https://shadowsocks.org/acl/android/v1/$route.acl").openStream().bufferedReader()
                .use { it.readText() }
        Acl.getFile(route).printWriter().use { it.write(acl) }
        Result.SUCCESS
    } catch (e: IOException) {
        e.printStackTrace()
        if (params.failureCount < AclSyncJob.MAX_RESCHEDULE) Result.RESCHEDULE else Result.FAILURE
    } catch (e: Exception) {    // unknown failures, probably shouldn't retry
        e.printStackTrace()
        Result.FAILURE
    }
}
