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

  def schedule(route: String) = new JobRequest.Builder(AclSyncJob.TAG + ':' + route)
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
      if(route != "self")
      {
        //noinspection JavaAccessorMethodCalledAsEmptyParen
        IOUtils.writeString(app.getApplicationInfo.dataDir + '/' + filename, autoClose(
          new URL("https://raw.githubusercontent.com/shadowsocksr/shadowsocksr-android/nokcp/src/main/assets/acl/" +
            filename).openConnection().getInputStream())(IOUtils.readString))
      }
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
