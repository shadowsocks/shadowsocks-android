/*
 * Shadowsocks - A shadowsocks client for Android
 * Copyright (C) 2014 <max.c.lv@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *                            ___====-_  _-====___
 *                      _--^^^#####//      \\#####^^^--_
 *                   _-^##########// (    ) \\##########^-_
 *                  -############//  |\^^/|  \\############-
 *                _/############//   (@::@)   \\############\_
 *               /#############((     \\//     ))#############\
 *              -###############\\    (oo)    //###############-
 *             -#################\\  / VV \  //#################-
 *            -###################\\/      \//###################-
 *           _#/|##########/\######(   /\   )######/\##########|\#_
 *           |/ |#/\#/\#/\/  \#/\##\  |  |  /##/\#/  \/\#/\#/\#| \|
 *           `  |/  V  V  `   V  \#\| |  | |/#/  V   '  V  V  \|  '
 *              `   `  `      `   / | |  | | \   '      '  '   '
 *                               (  | |  | |  )
 *                              __\ | |  | | /__
 *                             (vvv(VVV)(VVV)vvv)
 *
 *                              HERE BE DRAGONS
 *
 */
package com.github.shadowsocks.utils

import com.github.shadowsocks.ShadowsocksApplication.app
import okhttp3._
import java.util.concurrent.TimeUnit
import java.io.IOException
import com.github.shadowsocks.database._
import com.github.shadowsocks.utils.CloseUtils._
import com.github.shadowsocks.utils._
import android.util.{Base64, Log}
import android.widget.Toast
import android.content.Context
import android.os.Looper
import com.github.shadowsocks.R

object SSRSubUpdate {
  private val TAG = "Shadowsocks"

  def update(context:Context = null) = {
    if (app.settings.getInt(Key.ssrsub_autoupdate, 0) == 1) {
      Utils.ThrowableFuture {
        Looper.prepare()
        if (context != null) {
          Toast.makeText(context, app.resources.getString(R.string.ssrsub_toast_start), Toast.LENGTH_SHORT).show
        }
        app.ssrsubManager.getAllSSRSubs match {
          case Some(ssrsubs) =>
            var result = 1
            ssrsubs.foreach((ssrsub: SSRSub) => {

                var delete_profiles = app.profileManager.getAllProfilesByGroup(ssrsub.url_group) match {
                  case Some(profiles) =>
                    profiles
                  case _ => null
                }

                val builder = new OkHttpClient.Builder()
                                .connectTimeout(5, TimeUnit.SECONDS)
                                .writeTimeout(5, TimeUnit.SECONDS)
                                .readTimeout(5, TimeUnit.SECONDS)

                val client = builder.build();

                val request = new Request.Builder()
                  .url(ssrsub.url)
                  .build();

                try {
                  val response = client.newCall(request).execute()
                  val code = response.code()
                  if (code == 200) {
                    val response_string = new String(Base64.decode(response.body().string, Base64.URL_SAFE))
                    var limit_num = -1
                    var encounter_num = 0
                    if (response_string.indexOf("MAX=") == 0) {
                      limit_num = response_string.split("\\n")(0).split("MAX=")(1).replaceAll("\\D+","").toInt
                    }
                    var profiles_ssr = Parser.findAll_ssr(response_string)
                    profiles_ssr = scala.util.Random.shuffle(profiles_ssr)
                    profiles_ssr.foreach((profile: Profile) => {
                      if (encounter_num < limit_num && limit_num != -1 || limit_num == -1) {
                        val result_id = app.profileManager.createProfile_sub(profile)
                        if (result_id != 0) {
                          delete_profiles = delete_profiles.filter(_.id != result_id)
                        }
                      }
                      encounter_num += 1
                    })

                    delete_profiles.foreach((profile: Profile) => {
                      if (profile.id != app.profileId) {
                        app.profileManager.delProfile(profile.id)
                      }
                    })
                  } else throw new Exception("error")
                  response.body().close()
                } catch {
                  case e: IOException => {
                    result = 0
                  }
                }
            })
            if (context != null) {
              if (result == 1) {
                Toast.makeText(context, app.resources.getString(R.string.ssrsub_toast_success), Toast.LENGTH_SHORT).show
                Log.i(TAG, "update ssr sub successfully!")
              } else {
                Toast.makeText(context, app.resources.getString(R.string.ssrsub_toast_fail), Toast.LENGTH_SHORT).show
                Log.i(TAG, "update ssr sub failed!")
              }
            }
          case _ => {
          }
        }
        Looper.loop()
      }
    }
  }
}
