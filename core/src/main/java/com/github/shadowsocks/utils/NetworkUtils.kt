package com.github.shadowsocks.utils

import android.R.attr.host
import java.io.BufferedReader
import java.io.InputStreamReader
import java.util.regex.Pattern


/*******************************************************************************
 *                                                                             *
 *  Copyright (C) 2018 by https://github.com/shadowsocks/shadowsocks-android   *
 *  Copyright (C) 2018 by https://github.com/shadowsocks/shadowsocks-android   *
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


/**
 * The result set returned by Ping
 * @field type is the server working properly? true is normal, false is error
 * @field time is request server time
 * @field ttl Time To Live
 */
data class PingResult(val type:Boolean ,val time:String ,val ttl:String )

/**
 * @author zhangj
 * @describe Tool classes for dealing with all aspects of the network
 */
class NetworkUtils {
    companion object{
        fun ping(host:String ):PingResult{
            val runtime = Runtime.getRuntime()
            val proc = runtime.exec("ping -c 1 $host")
            proc.waitFor()
            val reader = BufferedReader(InputStreamReader(proc.inputStream))
            val pingTxt = reader.readText()
            when(proc.exitValue()){
                0->{
                    return PingResult(true,getPingTime(pingTxt),getPingTTL(pingTxt))
                }
            }
            // Default return request was aborted
            return PingResult(false, pingTxt,pingTxt)
        }
        private fun getPingTime(result:String): String{
            val matcher = Pattern.compile("time=.*").matcher(result)
            if(matcher.find()){
                return matcher.group().replace("time=","")
            }
            return ""
        }

        private fun getPingTTL(result:String): String{
            val matcher = Pattern.compile("ttl=\\d*").matcher(result)
            if(matcher.find()){
                return matcher.group().replace("ttl=","")
            }
            return ""
        }
    }

}