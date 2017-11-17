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

package com.github.shadowsocks

import android.content.{ComponentName, Context, Intent, ServiceConnection}
import android.os.{IBinder, RemoteException}
import com.github.shadowsocks.aidl.{IShadowsocksService, IShadowsocksServiceCallback}
import com.github.shadowsocks.utils.Action
import com.github.shadowsocks.ShadowsocksApplication.app
import com.github.shadowsocks.bg.{TransproxyService, VpnService}

/**
  * @author Mygod
  */
trait ServiceBoundContext extends Context with IBinder.DeathRecipient {
  private class ShadowsocksServiceConnection extends ServiceConnection {
    override def onServiceConnected(name: ComponentName, service: IBinder) {
      binder = service
      service.linkToDeath(ServiceBoundContext.this, 0)
      bgService = IShadowsocksService.Stub.asInterface(service)
      if (callback != null && !callbackRegistered) try {
        bgService.registerCallback(callback)
        callbackRegistered = true
        if (listeningForBandwidth) bgService.startListeningForBandwidth(callback)
      } catch {
        case _: RemoteException => // Nothing
      }
      ServiceBoundContext.this.onServiceConnected()
    }
    override def onServiceDisconnected(name: ComponentName) {
      unregisterCallback()
      ServiceBoundContext.this.onServiceDisconnected()
      bgService = null
      binder = null
    }
  }

  protected def setListeningForBandwidth(value: Boolean) {
    if (listeningForBandwidth != value && bgService != null && callback != null)
      if (value) bgService.startListeningForBandwidth(callback) else bgService.stopListeningForBandwidth(callback)
    listeningForBandwidth = value
  }

  private def unregisterCallback() {
    if (bgService != null && callback != null && callbackRegistered) try bgService.unregisterCallback(callback) catch {
      case _: RemoteException =>
    }
    callbackRegistered = false
  }

  protected def onServiceConnected(): Unit = ()
  /**
    * Different from Android framework, this method will be called even when you call `detachService`.
    */
  protected def onServiceDisconnected(): Unit = ()
  override def binderDied(): Unit = ()

  private var callback: IShadowsocksServiceCallback.Stub = _
  private var connection: ShadowsocksServiceConnection = _
  private var callbackRegistered: Boolean = _
  private var listeningForBandwidth: Boolean = _

  // Variables
  private var binder: IBinder = _
  var bgService: IShadowsocksService = _

  protected def attachService(callback: IShadowsocksServiceCallback.Stub = null) {
    this.callback = callback
    if (bgService == null) {
      val intent = new Intent(this, app.serviceClass)
      intent.setAction(Action.SERVICE)

      connection = new ShadowsocksServiceConnection()
      bindService(intent, connection, Context.BIND_AUTO_CREATE)
    }
  }

  protected def detachService() {
    unregisterCallback()
    onServiceDisconnected()
    callback = null
    if (connection != null) {
      try unbindService(connection) catch {
        case _: IllegalArgumentException => // ignore
      }
      connection = null
    }
    if (binder != null) {
      binder.unlinkToDeath(this, 0)
      binder = null
    }
    bgService = null
  }
}
