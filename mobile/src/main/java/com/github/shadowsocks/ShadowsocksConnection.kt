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

package com.github.shadowsocks

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.os.IBinder
import android.os.RemoteException
import com.github.shadowsocks.aidl.IShadowsocksService
import com.github.shadowsocks.aidl.IShadowsocksServiceCallback
import com.github.shadowsocks.bg.BaseService
import com.github.shadowsocks.utils.Action
import java.util.*

class ShadowsocksConnection(private val instance: Interface) : ServiceConnection {
    companion object {
        private val connections = WeakHashMap<Interface, ShadowsocksConnection>()
    }

    interface Interface : IBinder.DeathRecipient {
        val serviceCallback: IShadowsocksServiceCallback? get() = null
        val connection: ShadowsocksConnection
            get() = connections.getOrPut(this, { ShadowsocksConnection(this) })
        val listenForDeath get() = false

        fun onServiceConnected(service: IShadowsocksService) { }
        /**
         * Different from Android framework, this method will be called even when you call `detachService`.
         */
        fun onServiceDisconnected() { }
        override fun binderDied() { }
    }

    private var connectionActive = false
    private var callbackRegistered = false
    private var binder: IBinder? = null

    var listeningForBandwidth = false
        set(value) {
            val service = service
            if (listeningForBandwidth != value && service != null && instance.serviceCallback != null)
                if (value) service.startListeningForBandwidth(instance.serviceCallback)
                else service.stopListeningForBandwidth(instance.serviceCallback)
            field = value
        }
    var service: IShadowsocksService? = null

    override fun onServiceConnected(name: ComponentName?, binder: IBinder) {
        this.binder = binder
        if (instance.listenForDeath) binder.linkToDeath(instance, 0)
        val service = IShadowsocksService.Stub.asInterface(binder)!!
        this.service = service
        if (instance.serviceCallback != null && !callbackRegistered) try {
            service.registerCallback(instance.serviceCallback)
            callbackRegistered = true
            if (listeningForBandwidth) service.startListeningForBandwidth(instance.serviceCallback)
        } catch (_: RemoteException) { }
        instance.onServiceConnected(service)
    }

    override fun onServiceDisconnected(name: ComponentName?) {
        unregisterCallback()
        instance.onServiceDisconnected()
        service = null
        binder = null
    }

    private fun unregisterCallback() {
        val service = service
        if (service != null && instance.serviceCallback != null && callbackRegistered) try {
            service.unregisterCallback(instance.serviceCallback)
        } catch (_: RemoteException) { }
        callbackRegistered = false
    }

    fun connect() {
        if (connectionActive) return
        connectionActive = true
        val intent = Intent(instance as Context, BaseService.serviceClass.java).setAction(Action.SERVICE)
        instance.bindService(intent, this, Context.BIND_AUTO_CREATE)
    }

    fun disconnect() {
        unregisterCallback()
        instance.onServiceDisconnected()
        if (connectionActive) try {
            (instance as Context).unbindService(this)
        } catch (_: IllegalArgumentException) { }   // ignore
        connectionActive = false
        if (instance.listenForDeath) binder?.unlinkToDeath(instance, 0)
        binder = null
        service = null
    }
}
