package com.github.shadowsocks.unrealvpn

import android.os.Bundle
import android.os.RemoteException
import android.view.KeyEvent
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.WindowCompat
import androidx.preference.PreferenceDataStore
import com.github.shadowsocks.Core
import com.github.shadowsocks.R
import com.github.shadowsocks.aidl.IShadowsocksService
import com.github.shadowsocks.aidl.ShadowsocksConnection
import com.github.shadowsocks.bg.BaseService
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.preference.OnPreferenceDataStoreChangeListener
import com.github.shadowsocks.utils.Key
import com.github.shadowsocks.utils.StartService
import com.google.android.material.snackbar.Snackbar

abstract class BridgeActivity : AppCompatActivity(), ShadowsocksConnection.Callback,
    OnPreferenceDataStoreChangeListener {


    // service
    var state = BaseService.State.Idle

    override fun stateChanged(state: BaseService.State, profileName: String?, msg: String?) {
        changeState(state, msg)
    }

    override fun trafficPersisted(profileId: Long) = Unit

    private fun changeState(
        state: BaseService.State,
        msg: String? = null,
        animate: Boolean = true
    ) {
        showState(state, animate)
        if (msg != null) {
            createSnackbar(getString(R.string.vpn_error, msg)).show()
        }

        this.state = state
        stateListener?.invoke(state)
    }

    abstract fun showState(
        newState: BaseService.State,
        animate: Boolean = true
    )

    protected fun toggle() {
        if (state.canStop) Core.stopService() else connect.launch(null)
    }

    private val connection = ShadowsocksConnection(true)
    override fun onServiceConnected(service: IShadowsocksService) = changeState(
        try {
            BaseService.State.values()[service.state]
        } catch (_: RemoteException) {
            BaseService.State.Idle
        }
    )

    override fun onPreferenceDataStoreChanged(store: PreferenceDataStore, key: String) {
        when (key) {
            Key.serviceMode -> {
                connection.disconnect(this)
                connection.connect(this, this)
            }
        }
    }

    override fun onServiceDisconnected() = changeState(BaseService.State.Idle)
    override fun onBinderDied() {
        connection.disconnect(this)
        connection.connect(this, this)
    }

    abstract fun createSnackbar(message: String = ""): Snackbar

    private val connect = registerForActivityResult(StartService()) { permissionDenied ->
        if (permissionDenied) {
            createSnackbar().setText(R.string.vpn_permission_denied).show()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        createView()
        WindowCompat.setDecorFitsSystemWindows(window, false)
        changeState(BaseService.State.Idle, animate = false)
        connection.connect(this, this)
        DataStore.publicStore.registerChangeListener(this)
    }

    abstract fun createView()

    override fun onStart() {
        super.onStart()
        connection.bandwidthTimeout = 500
    }

    override fun onKeyShortcut(keyCode: Int, event: KeyEvent): Boolean {
        return when {
            keyCode == KeyEvent.KEYCODE_G && event.hasModifiers(KeyEvent.META_CTRL_ON) -> {
                toggle()
                true
            }

            keyCode == KeyEvent.KEYCODE_T && event.hasModifiers(KeyEvent.META_CTRL_ON) -> {
                // stats.testConnection()
                true
            }

            else -> false
        }
    }

    override fun onStop() {
        connection.bandwidthTimeout = 0
        super.onStop()
    }

    override fun onDestroy() {
        super.onDestroy()
        DataStore.publicStore.unregisterChangeListener(this)
        connection.disconnect(this)
    }

    companion object {
        var stateListener: ((BaseService.State) -> Unit)? = null
    }
}
