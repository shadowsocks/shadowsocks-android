/*******************************************************************************/
/*                                                                             */
/*  Copyright (C) 2016 by Max Lv <max.c.lv@gmail.com>                          */
/*  Copyright (C) 2016 by Mygod Studio <contact-shadowsocks-android@mygod.be>  */
/*                                                                             */
/*  This program is free software: you can redistribute it and/or modify       */
/*  it under the terms of the GNU General Public License as published by       */
/*  the Free Software Foundation, either version 3 of the License, or          */
/*  (at your option) any later version.                                        */
/*                                                                             */
/*  This program is distributed in the hope that it will be useful,            */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of             */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              */
/*  GNU General Public License for more details.                               */
/*                                                                             */
/*  You should have received a copy of the GNU General Public License          */
/*  along with this program. If not, see <http://www.gnu.org/licenses/>.       */
/*                                                                             */
/*******************************************************************************/

package com.github.shadowsocks.job

import java.io.IOException
import java.net.URL
import java.util.concurrent.TimeUnit

import com.evernote.android.job.Job.{Params, Result}
import com.evernote.android.job.{Job, JobRequest}
import com.github.shadowsocks.ShadowsocksApplication.app
import com.github.shadowsocks.utils.CloseUtils._
import com.github.shadowsocks.utils.IOUtils

/**
  * @author Mygod
  */
object AclSyncJob {
  final val TAG = "AclSyncJob"

  def schedule(route: String): Int = new JobRequest.Builder(AclSyncJob.TAG + ':' + route)
    .setExecutionWindow(1, TimeUnit.DAYS.toMillis(28))
    .setRequirementsEnforced(true)
    .setRequiredNetworkType(JobRequest.NetworkType.UNMETERED)
    .setRequiresCharging(true)
    .setUpdateCurrent(true)
    .build().schedule()
}

class AclSyncJob(route: String) extends Job {
  override def onRunJob(params: Params): Result = {
    val filename = route + ".acl"
    try {
      //noinspection JavaAccessorMethodCalledAsEmptyParen
      IOUtils.writeString(app.getApplicationInfo.dataDir + '/' + filename, autoClose(
        new URL("https://shadowsocks.org/acl/android/v1/" +
          filename).openConnection().getInputStream())(IOUtils.readString))
      Result.SUCCESS
    } catch {
      case e: IOException =>
        e.printStackTrace()
        Result.RESCHEDULE
      case e: Exception =>  // unknown failures, probably shouldn't retry
        e.printStackTrace()
        Result.FAILURE
    }
  }
}
