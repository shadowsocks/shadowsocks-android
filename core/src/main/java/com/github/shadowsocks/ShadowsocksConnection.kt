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
import android.os.DeadObjectException
import android.os.IBinder
import android.os.RemoteException
import com.github.shadowsocks.aidl.IShadowsocksService
import com.github.shadowsocks.aidl.IShadowsocksServiceCallback
import com.github.shadowsocks.bg.BaseService
import com.github.shadowsocks.utils.Action

class ShadowsocksConnection(private val callback: Callback, private var listenForDeath: Boolean = false) :
        ServiceConnection, IBinder.DeathRecipient {
    interface Callback {
        val serviceCallback: IShadowsocksServiceCallback? get() = null

        fun onServiceConnected(service: IShadowsocksService)
        /**
         * Different from Android framework, this method will be called even when you call `detachService`.
         */
        fun onServiceDisconnected() { }
        fun onBinderDied() { }
    }

    private var connectionActive = false
    private var callbackRegistered = false
    private var binder: IBinder? = null

    var listeningForBandwidth = false
        set(value) {
            val service = service
            if (listeningForBandwidth != value && service != null && callback.serviceCallback != null)
                if (value) service.startListeningForBandwidth(callback.serviceCallback) else try {
                    service.stopListeningForBandwidth(callback.serviceCallback)
                } catch (_: DeadObjectException) { }
            field = value
        }
    var service: IShadowsocksService? = null

    override fun onServiceConnected(name: ComponentName?, binder: IBinder) {
        this.binder = binder
        if (listenForDeath) binder.linkToDeath(this, 0)
        val service = IShadowsocksService.Stub.asInterface(binder)!!
        this.service = service
        if (callback.serviceCallback != null && !callbackRegistered) try {
            service.registerCallback(callback.serviceCallback)
            callbackRegistered = true
            if (listeningForBandwidth) service.startListeningForBandwidth(callback.serviceCallback)
        } catch (_: RemoteException) { }
        callback.onServiceConnected(service)
    }

    override fun onServiceDisconnected(name: ComponentName?) {
        unregisterCallback()
        callback.onServiceDisconnected()
        service = null
        binder = null
    }

    override fun binderDied() {
        service = null
        callback.onBinderDied()
    }

    private fun unregisterCallback() {
        val service = service
        if (service != null && callback.serviceCallback != null && callbackRegistered) try {
            service.unregisterCallback(callback.serviceCallback)
        } catch (_: RemoteException) { }
        callbackRegistered = false
    }

    fun connect(context: Context) {
        if (connectionActive) return
        connectionActive = true
        val intent = Intent(context, BaseService.serviceClass.java).setAction(Action.SERVICE)
        context.bindService(intent, this, Context.BIND_AUTO_CREATE)
    }

    fun disconnect(context: Context) {
        unregisterCallback()
        callback.onServiceDisconnected()
        if (connectionActive) try {
            context.unbindService(this)
        } catch (_: IllegalArgumentException) { }   // ignore
        connectionActive = false
        if (listenForDeath) binder?.unlinkToDeath(this, 0)
        binder = null
        if (callback.serviceCallback != null) service?.stopListeningForBandwidth(callback.serviceCallback)
        service = null
    }
}
