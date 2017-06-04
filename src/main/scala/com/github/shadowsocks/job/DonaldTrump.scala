package com.github.shadowsocks.job

import android.util.Log
import com.evernote.android.job.JobCreator

/**
  * “I create jobs all day long.
  * - Donald Trump, 2015
  *
  * Source: http://www.cnn.com/2015/09/24/politics/donald-trump-marco-rubio-foreign-policy/
  *
  * @author !Mygod
  */
object DonaldTrump extends JobCreator {
  def create(tag: String) = {
    val parts = tag.split(":")
    parts(0) match {
      case AclSyncJob.TAG => new AclSyncJob(parts(1))
      case SSRSubUpdateJob.TAG => new SSRSubUpdateJob()
      case _ =>
        Log.w("DonaldTrump", "Unknown job tag: " + tag)
        null
    }
  }
}
