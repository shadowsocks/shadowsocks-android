/*******************************************************************************/
/*                                                                             */
/*  Copyright (C) 2016 by Max Lv <max.c.lv@gmail.com>                          */
/*  Copyright (C) 2016 by Mygod Studio <contact-shadowsocks-android@mygod.be>  */
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
import android.os.{RemoteException, IBinder}
import com.github.shadowsocks.aidl.{IShadowsocksServiceCallback, IShadowsocksService}
import com.github.shadowsocks.utils.Action
import com.github.shadowsocks.ShadowsocksApplication.app

/**
  * @author Mygod
  */
trait ServiceBoundContext extends Context with IBinder.DeathRecipient {
  class ShadowsocksServiceConnection extends ServiceConnection {
    override def onServiceConnected(name: ComponentName, service: IBinder) {
      binder = service
      service.linkToDeath(ServiceBoundContext.this, 0)
      bgService = IShadowsocksService.Stub.asInterface(service)
      registerCallback()
      ServiceBoundContext.this.onServiceConnected()
    }
    override def onServiceDisconnected(name: ComponentName) {
      unregisterCallback()
      ServiceBoundContext.this.onServiceDisconnected()
      bgService = null
      binder = null
    }
  }

  def registerCallback(): Unit = if (bgService != null && callback != null && !callbackRegistered) try {
    bgService.registerCallback(callback)
    callbackRegistered = true
  } catch {
    case _: RemoteException => // Nothing
  }

  def unregisterCallback() {
    if (bgService != null && callback != null && callbackRegistered) try bgService.unregisterCallback(callback) catch {
      case _: RemoteException =>
    }
    callbackRegistered = false
  }

  def onServiceConnected(): Unit = ()
  def onServiceDisconnected(): Unit = ()
  override def binderDied(): Unit = ()

  private var callback: IShadowsocksServiceCallback.Stub = _
  private var connection: ShadowsocksServiceConnection = _
  private var callbackRegistered: Boolean = _

  // Variables
  var binder: IBinder = _
  var bgService: IShadowsocksService = _

  def attachService(callback: IShadowsocksServiceCallback.Stub = null) {
    this.callback = callback
    if (bgService == null) {
      val s = if (app.isNatEnabled) classOf[ShadowsocksNatService] else classOf[ShadowsocksVpnService]

      val intent = new Intent(this, s)
      intent.setAction(Action.SERVICE)

      connection = new ShadowsocksServiceConnection()
      bindService(intent, connection, Context.BIND_AUTO_CREATE)
    }
  }

  def detachService() {
    unregisterCallback()
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
