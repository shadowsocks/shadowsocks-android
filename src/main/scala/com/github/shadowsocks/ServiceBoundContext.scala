package com.github.shadowsocks

import android.content.{ComponentName, Context, Intent, ServiceConnection}
import android.os.{RemoteException, IBinder}
import com.github.shadowsocks.aidl.{IShadowsocksServiceCallback, IShadowsocksService}
import com.github.shadowsocks.utils.Action

/**
  * @author Mygod
  */
trait ServiceBoundContext extends Context {

  class ShadowsocksServiceConnection extends ServiceConnection {
    override def onServiceConnected(name: ComponentName, service: IBinder) {
      bgService = IShadowsocksService.Stub.asInterface(service)
      registerCallback
      ServiceBoundContext.this.onServiceConnected()
    }
    override def onServiceDisconnected(name: ComponentName) {
      if (callback != null) {
        unregisterCallback
        callback = null
      }
      ServiceBoundContext.this.onServiceDisconnected()
      bgService = null
    }
  }

  def registerCallback = if (bgService != null && callback != null && !callbackRegistered) try {
    bgService.registerCallback(callback)
    callbackRegistered = true
  } catch {
    case ignored: RemoteException => // Nothing
  }
  def unregisterCallback = if (bgService != null && callback != null && callbackRegistered) try {
    bgService.unregisterCallback(callback)
    callbackRegistered = false
  } catch {
    case ignored: RemoteException => // Nothing
  }

  def onServiceConnected() = ()
  def onServiceDisconnected() = ()

  private var callback: IShadowsocksServiceCallback.Stub = _
  private var connection: ShadowsocksServiceConnection = _
  private var callbackRegistered: Boolean = _

  // Variables
  var bgService: IShadowsocksService = _

  def attachService(callback: IShadowsocksServiceCallback.Stub = null) {
    this.callback = callback
    if (bgService == null) {
      val s =
        if (ShadowsocksApplication.isVpnEnabled) classOf[ShadowsocksVpnService] else classOf[ShadowsocksNatService]

      val intent = new Intent(this, s)
      intent.setAction(Action.SERVICE)

      connection = new ShadowsocksServiceConnection()
      bindService(intent, connection, Context.BIND_AUTO_CREATE)
    }
  }

  def deattachService() {
    if (bgService != null) {
      if (callback != null) {
        unregisterCallback
        callback = null
      }
      if (connection != null) {
        unbindService(connection)
        connection = null
      }
      bgService = null
    }
  }
}
