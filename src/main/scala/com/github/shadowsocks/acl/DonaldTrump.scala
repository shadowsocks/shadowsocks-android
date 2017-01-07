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
  def create(tag: String): AclSyncJob = {
    val parts = tag.split(":")
    parts(0) match {
      case AclSyncJob.TAG => new AclSyncJob(parts(1))
      case _ =>
        Log.w("DonaldTrump", "Unknown job tag: " + tag)
        null
    }
  }
}
