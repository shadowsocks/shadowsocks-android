package com.github.shadowsocks.flutter

import android.app.Activity
import android.os.Bundle
import android.os.RemoteException
import androidx.activity.result.contract.ActivityResultContracts
import com.github.shadowsocks.Core
import com.github.shadowsocks.aidl.IShadowsocksService
import com.github.shadowsocks.aidl.ShadowsocksConnection
import com.github.shadowsocks.aidl.TrafficStats
import com.github.shadowsocks.bg.BaseService
import com.github.shadowsocks.plugin.PluginContract
import com.github.shadowsocks.plugin.PluginManager
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.utils.Key
import com.github.shadowsocks.utils.StartService
import io.flutter.embedding.android.FlutterFragmentActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.EventChannel
import io.flutter.plugin.common.MethodChannel

class FlutterBridgeActivity : FlutterFragmentActivity(), ShadowsocksConnection.Callback {

    private val connection = ShadowsocksConnection(true)
    private var state = BaseService.State.Idle
    private var pendingResult: MethodChannel.Result? = null
    private var pendingPluginResult: MethodChannel.Result? = null

    // Event sinks for streaming data to Flutter
    var stateEventSink: EventChannel.EventSink? = null
    var trafficEventSink: EventChannel.EventSink? = null

    private val connect = registerForActivityResult(StartService()) { denied ->
        val result = pendingResult
        pendingResult = null
        if (denied) {
            result?.success(false)
        } else {
            result?.success(true)
        }
    }

    private val configurePlugin = registerForActivityResult(
        ActivityResultContracts.StartActivityForResult()
    ) { activityResult ->
        val result = pendingPluginResult
        pendingPluginResult = null
        when (activityResult.resultCode) {
            Activity.RESULT_OK -> {
                val options = activityResult.data?.getStringExtra(PluginContract.EXTRA_OPTIONS)
                result?.success(mapOf("status" to "ok", "options" to (options ?: "")))
            }
            PluginContract.RESULT_FALLBACK -> {
                result?.success(mapOf("status" to "fallback"))
            }
            else -> {
                result?.success(mapOf("status" to "cancelled"))
            }
        }
    }

    override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
        super.configureFlutterEngine(flutterEngine)

        val messenger = flutterEngine.dartExecutor.binaryMessenger

        // Service control channel
        MethodChannel(messenger, "com.github.shadowsocks/service").setMethodCallHandler { call, result ->
            when (call.method) {
                "getState" -> result.success(state.ordinal)
                "toggle" -> handleToggle(result)
                "requestVpnPermission" -> {
                    pendingResult = result
                    connect.launch(null)
                }
                "configurePlugin" -> {
                    val pluginId = call.argument<String>("pluginId") ?: ""
                    val options = call.argument<String>("options") ?: ""
                    val intent = PluginManager.buildIntent(pluginId, PluginContract.ACTION_CONFIGURE)
                    if (intent.resolveActivity(packageManager) != null) {
                        pendingPluginResult = result
                        configurePlugin.launch(
                            intent.putExtra(PluginContract.EXTRA_OPTIONS, options)
                        )
                    } else {
                        result.success(mapOf("status" to "fallback"))
                    }
                }
                "testConnection" -> result.success(null) // TODO: implement HttpsTest
                else -> result.notImplemented()
            }
        }

        // State event channel
        EventChannel(messenger, "com.github.shadowsocks/state").setStreamHandler(
            object : EventChannel.StreamHandler {
                override fun onListen(arguments: Any?, events: EventChannel.EventSink?) {
                    stateEventSink = events
                }
                override fun onCancel(arguments: Any?) {
                    stateEventSink = null
                }
            }
        )

        // Traffic event channel
        EventChannel(messenger, "com.github.shadowsocks/traffic").setStreamHandler(
            object : EventChannel.StreamHandler {
                override fun onListen(arguments: Any?, events: EventChannel.EventSink?) {
                    trafficEventSink = events
                }
                override fun onCancel(arguments: Any?) {
                    trafficEventSink = null
                }
            }
        )

        // Profile channel
        ProfileChannelHandler.register(messenger)

        // Settings channel
        SettingsChannelHandler.register(messenger)

        // App list channel
        AppListChannelHandler.register(messenger, this)
    }

    private fun handleToggle(result: MethodChannel.Result) {
        if (state.canStop) {
            Core.stopService()
            result.success(true)
        } else {
            pendingResult = result
            connect.launch(null)
        }
    }

    // --- ShadowsocksConnection.Callback ---

    override fun stateChanged(state: BaseService.State, profileName: String?, msg: String?) {
        this.state = state
        runOnUiThread {
            stateEventSink?.success(mapOf(
                "state" to state.ordinal,
                "profileName" to profileName,
                "message" to msg,
            ))
        }
    }

    override fun trafficUpdated(profileId: Long, stats: TrafficStats) {
        runOnUiThread {
            trafficEventSink?.success(mapOf(
                "profileId" to profileId,
                "txRate" to stats.txRate,
                "rxRate" to stats.rxRate,
                "txTotal" to stats.txTotal,
                "rxTotal" to stats.rxTotal,
            ))
        }
    }

    override fun trafficPersisted(profileId: Long) {
        // Traffic persisted, Flutter can refresh if needed
    }

    override fun onServiceConnected(service: IShadowsocksService) {
        val newState = try {
            BaseService.State.entries[service.state]
        } catch (_: RemoteException) {
            BaseService.State.Idle
        }
        stateChanged(newState, null, null)
    }

    override fun onServiceDisconnected() = stateChanged(BaseService.State.Idle, null, null)

    override fun onBinderDied() {
        connection.disconnect(this)
        connection.connect(this, this)
    }

    // --- Lifecycle ---

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        connection.connect(this, this)
        DataStore.publicStore.registerChangeListener(object :
            com.github.shadowsocks.preference.OnPreferenceDataStoreChangeListener {
            override fun onPreferenceDataStoreChanged(
                store: androidx.preference.PreferenceDataStore, key: String
            ) {
                if (key == Key.serviceMode) {
                    connection.disconnect(this@FlutterBridgeActivity)
                    connection.connect(this@FlutterBridgeActivity, this@FlutterBridgeActivity)
                }
            }
        })
    }

    override fun onStart() {
        super.onStart()
        connection.bandwidthTimeout = 500
    }

    override fun onStop() {
        connection.bandwidthTimeout = 0
        super.onStop()
    }

    override fun onDestroy() {
        super.onDestroy()
        connection.disconnect(this)
    }
}
