/*******************************************************************************/
/*                                                                             */
/*  Copyright (C) 2017 by Max Lv <max.c.lv@gmail.com>                          */
/*  Copyright (C) 2017 by Mygod Studio <contact-shadowsocks-android@mygod.be>  */
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

package com.github.shadowsocks.acl

import java.io.IOException
import java.util.concurrent.TimeUnit

import com.evernote.android.job.Job.{Params, Result}
import com.evernote.android.job.{Job, JobRequest}
import com.github.shadowsocks.utils.IOUtils

import scala.io.Source

/**
  * @author Mygod
  */
object AclSyncJob {
  final val TAG = "AclSyncJob"
  final val MAX_RESCHEDULE = 3

  def schedule(route: String): Int = new JobRequest.Builder(AclSyncJob.TAG + ':' + route)
    .setExecutionWindow(TimeUnit.SECONDS.toMillis(10), TimeUnit.DAYS.toMillis(28))
    .setRequirementsEnforced(true)
    .setRequiredNetworkType(JobRequest.NetworkType.UNMETERED)
    .setRequiresCharging(true)
    .setUpdateCurrent(true)
    .build().schedule()
}

class AclSyncJob(route: String) extends Job {
  override def onRunJob(params: Params): Result = {
    try {
      //noinspection JavaAccessorMethodCalledAsEmptyParen
      IOUtils.writeString(Acl.getFile(route),
        Source.fromURL("https://shadowsocks.org/acl/android/v1/" + route + ".acl").mkString)
      Result.SUCCESS
    } catch {
      case e: IOException =>
        e.printStackTrace()
        if (params.getFailureCount < AclSyncJob.MAX_RESCHEDULE) Result.RESCHEDULE else Result.FAILURE
      case e: Exception =>  // unknown failures, probably shouldn't retry
        e.printStackTrace()
        Result.FAILURE
    }
  }
}
