package com.github.shadowsocks

import android.os.RemoteCallbackList
import com.github.shadowsocks.aidl.{Config, IShadowsocksService, IShadowsocksServiceCallback}
import com.github.shadowsocks.utils.{Path, State}
import java.io.{IOException, FileNotFoundException, FileReader, BufferedReader}
import android.util.Log
import android.app.Notification

trait BaseService {

  var state = State.INIT

  final val callbacks = new RemoteCallbackList[IShadowsocksServiceCallback]

  protected val binder = new IShadowsocksService.Stub {
    override def getMode: Int = {
      getServiceMode
    }

    override def getState: Int = {
      state
    }

    override def unregisterCallback(cb: IShadowsocksServiceCallback) {
      if (cb != null ) callbacks.unregister(cb)
    }

    override def registerCallback(cb: IShadowsocksServiceCallback) {
      if (cb != null) callbacks.register(cb)
    }

    override def stop() {
      stopRunner()
    }

    override def start(config: Config) {
      startRunner(config)
    }
  }

  def startRunner(config: Config)
  def stopRunner()
  def getServiceMode: Int
  def getTag: String

  def changeState(s: Int) {
    changeState(s, null)
  }

  protected def changeState(s: Int, msg: String) {
    if (state != s) {
      val n = callbacks.beginBroadcast()
      for (i <- 0 to n -1) {
        callbacks.getBroadcastItem(i).stateChanged(s, msg)
      }
      callbacks.finishBroadcast()
      state = s
    }
  }

  def getPid(name: String): Int = {
    try {
      val reader: BufferedReader = new BufferedReader(new FileReader(Path.BASE + name + ".pid"))
      val line = reader.readLine
      return Integer.valueOf(line)
    } catch {
      case e: FileNotFoundException =>
        Log.e(getTag, "Cannot open pid file: " + name)
      case e: IOException =>
        Log.e(getTag, "Cannot read pid file: " + name)
      case e: NumberFormatException =>
        Log.e(getTag, "Invalid pid", e)
    }
    -1
  }

  def initSoundVibrateLights(notification: Notification) {
    notification.sound = null
  }
}
