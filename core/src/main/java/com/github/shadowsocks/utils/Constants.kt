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

package com.github.shadowsocks.utils

object Key {
    /**
     * Public config that doesn't need to be kept secret.
     */
    const val DB_PUBLIC = "config.db"
    const val DB_PROFILE = "profile.db"

    const val id = "profileId"
    const val name = "profileName"

    const val individual = "Proxyed"

    const val serviceMode = "serviceMode"
    const val modeProxy = "proxy"
    const val modeVpn = "vpn"
    const val modeTransproxy = "transproxy"
    const val shareOverLan = "shareOverLan"
    const val portProxy = "portProxy"
    const val portLocalDns = "portLocalDns"
    const val portTransproxy = "portTransproxy"

    const val route = "route"

    const val persistAcrossReboot = "isAutoConnect"
    const val directBootAware = "directBootAware"

    const val proxyApps = "isProxyApps"
    const val bypass = "isBypassApps"
    const val udpdns = "isUdpDns"
    const val ipv6 = "isIpv6"
    const val metered = "metered"

    const val host = "proxy"
    const val password = "sitekey"
    const val method = "encMethod"
    const val remotePort = "remotePortNum"
    const val remoteDns = "remoteDns"

    const val plugin = "plugin"
    const val pluginConfigure = "plugin.configure"
    const val udpFallback = "udpFallback"

    const val dirty = "profileDirty"

    const val assetUpdateTime = "assetUpdateTime"

    // TV specific values
    const val controlStats = "control.stats"
    const val controlImport = "control.import"
    const val controlExport = "control.export"
    const val about = "about"
    const val aboutOss = "about.ossLicenses"
}

object Action {
    const val SERVICE = "com.github.shadowsocks.SERVICE"
    const val CLOSE = "com.github.shadowsocks.CLOSE"
    const val RELOAD = "com.github.shadowsocks.RELOAD"
    const val ABORT = "com.github.shadowsocks.ABORT"

    const val EXTRA_PROFILE_ID = "com.github.shadowsocks.EXTRA_PROFILE_ID"
}
