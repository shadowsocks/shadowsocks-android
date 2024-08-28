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

import android.annotation.SuppressLint
import android.annotation.TargetApi
import android.app.Application
import android.content.ComponentCallbacks
import android.content.Context
import android.content.res.Configuration

@SuppressLint("MissingSuperCall", "Registered")
@TargetApi(24)
class DeviceStorageApp(private val app: Application) : Application() {
    init {
        attachBaseContext(app.createDeviceProtectedStorageContext())
    }

    /**
     * Thou shalt not get the REAL underlying application context which would no longer be operating under device
     * protected storage.
     */
    override fun getApplicationContext(): Context = this

    override fun onCreate() = app.onCreate()
    override fun onTerminate() = app.onTerminate()
    override fun onConfigurationChanged(newConfig: Configuration) = app.onConfigurationChanged(newConfig)
    override fun onLowMemory() = app.onLowMemory()
    override fun onTrimMemory(level: Int) = app.onTrimMemory(level)
    override fun registerComponentCallbacks(callback: ComponentCallbacks?) = app.registerComponentCallbacks(callback)
    override fun unregisterComponentCallbacks(callback: ComponentCallbacks?) =
        app.unregisterComponentCallbacks(callback)
    override fun registerActivityLifecycleCallbacks(callback: ActivityLifecycleCallbacks?) =
        app.registerActivityLifecycleCallbacks(callback)
    override fun unregisterActivityLifecycleCallbacks(callback: ActivityLifecycleCallbacks?) =
        app.unregisterActivityLifecycleCallbacks(callback)
    override fun registerOnProvideAssistDataListener(callback: OnProvideAssistDataListener?) =
        app.registerOnProvideAssistDataListener(callback)
    override fun unregisterOnProvideAssistDataListener(callback: OnProvideAssistDataListener?) =
        app.unregisterOnProvideAssistDataListener(callback)
}
