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

import android.app.Activity
import android.app.PendingIntent
import android.app.UiModeManager
import android.app.backup.BackupManager
import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.net.VpnService
import android.nfc.NdefMessage
import android.nfc.NfcAdapter
import android.os.Bundle
import android.os.SystemClock
import android.text.format.Formatter
import android.util.Log
import android.view.Gravity
import android.view.MenuItem
import android.view.View
import android.widget.TextView
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.app.AppCompatDelegate
import androidx.browser.customtabs.CustomTabsIntent
import androidx.core.content.ContextCompat
import androidx.core.content.getSystemService
import androidx.core.net.toUri
import androidx.drawerlayout.widget.DrawerLayout
import androidx.preference.PreferenceDataStore
import com.crashlytics.android.Crashlytics
import com.github.shadowsocks.App.Companion.app
import com.github.shadowsocks.acl.Acl
import com.github.shadowsocks.acl.CustomRulesFragment
import com.github.shadowsocks.aidl.IShadowsocksService
import com.github.shadowsocks.aidl.IShadowsocksServiceCallback
import com.github.shadowsocks.bg.BaseService
import com.github.shadowsocks.bg.Executable
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.database.ProfileManager
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.preference.OnPreferenceDataStoreChangeListener
import com.github.shadowsocks.utils.Key
import com.github.shadowsocks.utils.responseLength
import com.github.shadowsocks.utils.thread
import com.github.shadowsocks.widget.ServiceButton
import com.google.android.material.navigation.NavigationView
import com.google.android.material.snackbar.Snackbar
import java.io.IOException
import java.net.HttpURLConnection
import java.net.InetSocketAddress
import java.net.Proxy
import java.net.URL
import java.util.*

class MainActivity : AppCompatActivity(), ShadowsocksConnection.Interface, OnPreferenceDataStoreChangeListener,
        NavigationView.OnNavigationItemSelectedListener {
    companion object {
        private const val TAG = "ShadowsocksMainActivity"
        private const val REQUEST_CONNECT = 1

        fun pendingIntent(context: Context) = PendingIntent.getActivity(context, 0,
                Intent(context, MainActivity::class.java).setFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT), 0)

        var stateListener: ((Int) -> Unit)? = null
    }

    // UI
    private lateinit var fab: ServiceButton
    internal lateinit var drawer: DrawerLayout
    private lateinit var navigation: NavigationView

    private var testCount = 0
    private lateinit var statusText: TextView
    private lateinit var txText: TextView
    private lateinit var rxText: TextView
    private lateinit var txRateText: TextView
    private lateinit var rxRateText: TextView

    private val customTabsIntent by lazy {
        CustomTabsIntent.Builder()
                .setToolbarColor(ContextCompat.getColor(this, R.color.color_primary))
                .build()
    }
    fun launchUrl(uri: String) = try {
        customTabsIntent.launchUrl(this, uri.toUri())
    } catch (_: ActivityNotFoundException) { }  // ignore

    // service
    var state = BaseService.IDLE
    override val serviceCallback: IShadowsocksServiceCallback.Stub by lazy {
        object : IShadowsocksServiceCallback.Stub() {
            override fun stateChanged(state: Int, profileName: String?, msg: String?) {
                app.handler.post { changeState(state, msg, true) }
            }
            override fun trafficUpdated(profileId: Long, txRate: Long, rxRate: Long, txTotal: Long, rxTotal: Long) {
                app.handler.post { updateTraffic(profileId, txRate, rxRate, txTotal, rxTotal) }
            }
            override fun trafficPersisted(profileId: Long) {
                app.handler.post { ProfilesFragment.instance?.onTrafficPersisted(profileId) }
            }
        }
    }

    fun changeState(state: Int, msg: String? = null, animate: Boolean = false) {
        fab.changeState(state, animate)
        when (state) {
            BaseService.CONNECTING -> statusText.setText(R.string.connecting)
            BaseService.CONNECTED -> statusText.setText(R.string.vpn_connected)
            BaseService.STOPPING -> statusText.setText(R.string.stopping)
            else -> {
                if (msg != null) {
                    Snackbar.make(findViewById(R.id.snackbar),
                            getString(R.string.vpn_error).format(Locale.ENGLISH, msg), Snackbar.LENGTH_LONG).show()
                }
                statusText.setText(R.string.not_connected)
            }
        }
        this.state = state
        if (state != BaseService.CONNECTED) {
            updateTraffic(-1, 0, 0, 0, 0)
            testCount += 1  // suppress previous test messages
        }
        ProfilesFragment.instance?.profilesAdapter?.notifyDataSetChanged()  // refresh button enabled state
        stateListener?.invoke(state)
    }
    fun updateTraffic(profileId: Long, txRate: Long, rxRate: Long, txTotal: Long, rxTotal: Long) {
        txText.text = Formatter.formatFileSize(this, txTotal)
        rxText.text = Formatter.formatFileSize(this, rxTotal)
        txRateText.text = getString(R.string.speed, Formatter.formatFileSize(this, txRate))
        rxRateText.text = getString(R.string.speed, Formatter.formatFileSize(this, rxRate))
        val child = supportFragmentManager.findFragmentById(R.id.fragment_holder) as ToolbarFragment?
        if (state != BaseService.STOPPING)
            child?.onTrafficUpdated(profileId, txRate, rxRate, txTotal, rxTotal)
    }

    /**
     * Based on: https://android.googlesource.com/platform/frameworks/base/+/97bfd27/services/core/java/com/android/server/connectivity/NetworkMonitor.java#879
     */
    private fun testConnection(id: Int) {
        val url = URL("https", when (app.currentProfile!!.route) {
            Acl.CHINALIST -> "www.qualcomm.cn"
            else -> "www.google.com"
        }, "/generate_204")
        val conn = (if (BaseService.usingVpnMode) url.openConnection() else
            url.openConnection(Proxy(Proxy.Type.SOCKS,
                    InetSocketAddress("127.0.0.1", DataStore.portProxy))))
                as HttpURLConnection
        conn.setRequestProperty("Connection", "close")
        conn.instanceFollowRedirects = false
        conn.useCaches = false
        val (success, result) = try {
            val start = SystemClock.elapsedRealtime()
            val code = conn.responseCode
            val elapsed = SystemClock.elapsedRealtime() - start
            if (code == 204 || code == 200 && conn.responseLength == 0L)
                Pair(true, getString(R.string.connection_test_available, elapsed))
            else throw IOException(getString(R.string.connection_test_error_status_code, code))
        } catch (e: IOException) {
            Pair(false, getString(R.string.connection_test_error, e.message))
        } finally {
            conn.disconnect()
        }
        if (testCount == id) app.handler.post {
            if (success) statusText.text = result else {
                statusText.setText(R.string.connection_test_fail)
                Snackbar.make(findViewById(R.id.snackbar), result, Snackbar.LENGTH_LONG).show()
            }
        }
    }

    override val listenForDeath: Boolean get() = true
    override fun onServiceConnected(service: IShadowsocksService) = changeState(service.state)
    override fun onServiceDisconnected() = changeState(BaseService.IDLE)
    override fun binderDied() {
        super.binderDied()
        app.handler.post {
            connection.disconnect()
            Executable.killAll()
            connection.connect()
        }
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        if (resultCode == Activity.RESULT_OK) app.startService() else {
            Snackbar.make(findViewById(R.id.snackbar), R.string.vpn_permission_denied, Snackbar.LENGTH_LONG).show()
            Crashlytics.log(Log.ERROR, TAG, "Failed to start VpnService from onActivityResult: $data")
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.layout_main)
        drawer = findViewById(R.id.drawer)
        navigation = findViewById(R.id.navigation)
        navigation.setNavigationItemSelectedListener(this)
        if (savedInstanceState == null) {
            navigation.menu.findItem(R.id.profiles).isChecked = true
            displayFragment(ProfilesFragment())
        }

        statusText = findViewById(R.id.status)
        txText = findViewById(R.id.tx)
        txRateText = findViewById(R.id.txRate)
        rxText = findViewById(R.id.rx)
        rxRateText = findViewById(R.id.rxRate)
        findViewById<View>(R.id.stat).setOnClickListener {
            if (state == BaseService.CONNECTED) {
                ++testCount
                statusText.setText(R.string.connection_test_testing)
                val id = testCount  // it would change by other code
                thread("ConnectionTest") { testConnection(id) }
            }
        }

        fab = findViewById(R.id.fab)
        fab.setOnClickListener {
            when {
                state == BaseService.CONNECTED -> app.stopService()
                BaseService.usingVpnMode -> {
                    val intent = VpnService.prepare(this)
                    if (intent != null) startActivityForResult(intent, REQUEST_CONNECT)
                    else onActivityResult(REQUEST_CONNECT, Activity.RESULT_OK, null)
                }
                else -> app.startService()
            }
        }

        changeState(BaseService.IDLE)   // reset everything to init state
        app.handler.post { connection.connect() }
        DataStore.publicStore.registerChangeListener(this)

        val intent = this.intent
        if (intent != null) handleShareIntent(intent)
        if (savedInstanceState != null
                && DataStore.nightMode == AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM
                && AppCompatDelegate.getDefaultNightMode() != AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM) {
            AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM)
        }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        handleShareIntent(intent)
    }
    private fun handleShareIntent(intent: Intent) {
        val sharedStr = when (intent.action) {
            Intent.ACTION_VIEW -> intent.data.toString()
            NfcAdapter.ACTION_NDEF_DISCOVERED -> {
                val rawMsgs = intent.getParcelableArrayExtra(NfcAdapter.EXTRA_NDEF_MESSAGES)
                if (rawMsgs != null && rawMsgs.isNotEmpty()) String((rawMsgs[0] as NdefMessage).records[0].payload)
                else null
            }
            else -> null
        }
        if (sharedStr.isNullOrEmpty()) return
        val profiles = Profile.findAll(sharedStr).toList()
        if (profiles.isEmpty()) {
            Snackbar.make(findViewById(R.id.snackbar), R.string.profile_invalid_input, Snackbar.LENGTH_LONG).show()
            return
        }
        AlertDialog.Builder(this)
                .setTitle(R.string.add_profile_dialog)
                .setPositiveButton(R.string.yes) { _, _ -> profiles.forEach { ProfileManager.createProfile(it) } }
                .setNegativeButton(R.string.no, null)
                .setMessage(profiles.joinToString("\n"))
                .create()
                .show()
    }

    override fun onPreferenceDataStoreChanged(store: PreferenceDataStore, key: String?) {
        when (key) {
            Key.serviceMode -> app.handler.post {
                connection.disconnect()
                connection.connect()
            }
            Key.nightMode -> {
                val mode = DataStore.nightMode
                AppCompatDelegate.setDefaultNightMode(when (mode) {
                    AppCompatDelegate.getDefaultNightMode() -> return
                    AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM -> getSystemService<UiModeManager>()!!.nightMode
                    else -> mode
                })
                recreate()
            }
        }
    }

    private fun displayFragment(fragment: ToolbarFragment) {
        supportFragmentManager.beginTransaction().replace(R.id.fragment_holder, fragment).commitAllowingStateLoss()
        drawer.closeDrawers()
    }

    override fun onNavigationItemSelected(item: MenuItem): Boolean {
        if (item.isChecked) drawer.closeDrawers() else {
            when (item.itemId) {
                R.id.profiles -> displayFragment(ProfilesFragment())
                R.id.globalSettings -> displayFragment(GlobalSettingsFragment())
                R.id.about -> {
                    app.analytics.logEvent("about", Bundle())
                    displayFragment(AboutFragment())
                }
                R.id.faq -> {
                    launchUrl(getString(R.string.faq_url))
                    return true
                }
                R.id.customRules -> displayFragment(CustomRulesFragment())
                else -> return false
            }
            item.isChecked = true
        }
        return true
    }

    override fun onResume() {
        super.onResume()
        app.remoteConfig.fetch()
    }

    override fun onStart() {
        super.onStart()
        connection.listeningForBandwidth = true
    }

    override fun onBackPressed() {
        if (drawer.isDrawerOpen(Gravity.START)) drawer.closeDrawers() else {
            val currentFragment = supportFragmentManager.findFragmentById(R.id.fragment_holder) as ToolbarFragment
            if (!currentFragment.onBackPressed())
                if (currentFragment is ProfilesFragment) super.onBackPressed()
                else navigation.menu.findItem(R.id.profiles).isChecked = true
        }
    }

    override fun onStop() {
        connection.listeningForBandwidth = false
        super.onStop()
    }

    override fun onDestroy() {
        super.onDestroy()
        DataStore.publicStore.unregisterChangeListener(this)
        connection.disconnect()
        BackupManager(this).dataChanged()
        app.handler.removeCallbacksAndMessages(null)
    }
}
