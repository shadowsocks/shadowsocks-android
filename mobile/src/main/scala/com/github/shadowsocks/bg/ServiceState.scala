package com.github.shadowsocks.bg

/**
  * @author Mygod
  */
object ServiceState {
  /**
    * This state will never be broadcast by the service. This state is only used to indicate that the current context
    * hasn't bound to any context.
    */
  val IDLE = 0
  val CONNECTING = 1
  val CONNECTED = 2
  val STOPPING = 3
  val STOPPED = 4
}
