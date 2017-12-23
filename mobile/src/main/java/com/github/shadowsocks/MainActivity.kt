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
import android.app.backup.BackupManager
import android.content.ActivityNotFoundException
import android.content.Intent
import android.net.Uri
import android.net.VpnService
import android.nfc.NdefMessage
import android.nfc.NfcAdapter
import android.os.Bundle
import android.os.SystemClock
import android.support.customtabs.CustomTabsIntent
import android.support.design.widget.Snackbar
import android.support.v4.content.ContextCompat
import android.support.v7.app.AlertDialog
import android.support.v7.app.AppCompatActivity
import android.support.v7.content.res.AppCompatResources
import android.support.v7.preference.PreferenceDataStore
import android.util.Log
import android.view.View
import android.widget.TextView
import com.github.shadowsocks.App.Companion.app
import com.github.shadowsocks.acl.Acl
import com.github.shadowsocks.acl.CustomRulesFragment
import com.github.shadowsocks.aidl.IShadowsocksService
import com.github.shadowsocks.aidl.IShadowsocksServiceCallback
import com.github.shadowsocks.bg.BaseService
import com.github.shadowsocks.bg.Executable
import com.github.shadowsocks.bg.TrafficMonitor
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.database.ProfileManager
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.preference.OnPreferenceDataStoreChangeListener
import com.github.shadowsocks.utils.Key
import com.github.shadowsocks.utils.responseLength
import com.github.shadowsocks.utils.thread
import com.github.shadowsocks.widget.ServiceButton
import com.mikepenz.crossfader.Crossfader
import com.mikepenz.crossfader.view.CrossFadeSlidingPaneLayout
import com.mikepenz.materialdrawer.Drawer
import com.mikepenz.materialdrawer.DrawerBuilder
import com.mikepenz.materialdrawer.interfaces.ICrossfader
import com.mikepenz.materialdrawer.model.PrimaryDrawerItem
import com.mikepenz.materialdrawer.model.interfaces.IDrawerItem
import java.net.HttpURLConnection
import java.net.InetSocketAddress
import java.net.Proxy
import java.net.URL
import java.util.*

class MainActivity : AppCompatActivity(), ShadowsocksConnection.Interface, Drawer.OnDrawerItemClickListener,
        OnPreferenceDataStoreChangeListener {
    companion object {
        private const val TAG = "ShadowsocksMainActivity"
        private const val REQUEST_CONNECT = 1

        private const val DRAWER_PROFILES = 0L
        private const val DRAWER_GLOBAL_SETTINGS = 1L
        private const val DRAWER_ABOUT = 3L
        private const val DRAWER_FAQ = 4L
        private const val DRAWER_CUSTOM_RULES = 5L

        var stateListener: ((Int) -> Unit)? = null
    }

    // UI
    private lateinit var fab: ServiceButton
    internal var crossfader: Crossfader<CrossFadeSlidingPaneLayout>? = null
    internal lateinit var drawer: Drawer
    private var previousSelectedDrawer: Long = 0    // it's actually lateinit

    private var testCount = 0
    private lateinit var statusText: TextView
    private lateinit var txText: TextView
    private lateinit var rxText: TextView
    private lateinit var txRateText: TextView
    private lateinit var rxRateText: TextView

    private val customTabsIntent by lazy {
        CustomTabsIntent.Builder()
                .setToolbarColor(ContextCompat.getColor(this, R.color.material_primary_500))
                .build()
    }
    fun launchUrl(uri: Uri) = try {
        customTabsIntent.launchUrl(this, uri)
    } catch (_: ActivityNotFoundException) { }  // ignore
    fun launchUrl(uri: String) = launchUrl(Uri.parse(uri))

    // service
    var state = BaseService.IDLE
    override val serviceCallback: IShadowsocksServiceCallback.Stub by lazy {
        object : IShadowsocksServiceCallback.Stub() {
            override fun stateChanged(state: Int, profileName: String?, msg: String?) {
                app.handler.post { changeState(state, msg, true) }
            }
            override fun trafficUpdated(profileId: Int, txRate: Long, rxRate: Long, txTotal: Long, rxTotal: Long) {
                app.handler.post { updateTraffic(profileId, txRate, rxRate, txTotal, rxTotal) }
            }
            override fun trafficPersisted(profileId: Int) {
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
                    Log.e(TAG, "Error to start VPN service: $msg")
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
    fun updateTraffic(profileId: Int, txRate: Long, rxRate: Long, txTotal: Long, rxTotal: Long) {
        txText.text = TrafficMonitor.formatTraffic(txTotal)
        rxText.text = TrafficMonitor.formatTraffic(rxTotal)
        txRateText.text = getString(R.string.speed, TrafficMonitor.formatTraffic(txRate))
        rxRateText.text = getString(R.string.speed, TrafficMonitor.formatTraffic(rxRate))
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
        conn.instanceFollowRedirects = false
        conn.connectTimeout = 10000
        conn.readTimeout = 10000
        conn.useCaches = false
        val (success, result) = try {
            val start = SystemClock.elapsedRealtime()
            val code = conn.responseCode
            val elapsed = SystemClock.elapsedRealtime() - start
            if (code == 204 || code == 200 && conn.responseLength == 0L)
                Pair(true, getString(R.string.connection_test_available, elapsed))
            else throw Exception(getString(R.string.connection_test_error_status_code, code))
        } catch (e: Exception) {
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
        app.handler.post {
            connection.disconnect()
            Executable.killAll()
            connection.connect()
        }
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        if (resultCode == Activity.RESULT_OK) app.startService() else {
            Snackbar.make(findViewById(R.id.snackbar), R.string.vpn_permission_denied, Snackbar.LENGTH_LONG).show()
            Log.e(TAG, "Failed to start VpnService: $data")
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.layout_main)
        val drawerBuilder = DrawerBuilder()
                .withActivity(this)
                .withTranslucentStatusBar(true)
                .withHeader(R.layout.layout_header)
                .addDrawerItems(
                        PrimaryDrawerItem()
                                .withIdentifier(DRAWER_PROFILES)
                                .withName(R.string.profiles)
                                .withIcon(AppCompatResources.getDrawable(this, R.drawable.ic_action_description))
                                .withIconTintingEnabled(true),
                        PrimaryDrawerItem()
                                .withIdentifier(DRAWER_CUSTOM_RULES)
                                .withName(R.string.custom_rules)
                                .withIcon(AppCompatResources.getDrawable(this, R.drawable.ic_action_assignment))
                                .withIconTintingEnabled(true),
                        PrimaryDrawerItem()
                                .withIdentifier(DRAWER_GLOBAL_SETTINGS)
                                .withName(R.string.settings)
                                .withIcon(AppCompatResources.getDrawable(this, R.drawable.ic_action_settings))
                                .withIconTintingEnabled(true)
                )
                .addStickyDrawerItems(
                        PrimaryDrawerItem()
                                .withIdentifier(DRAWER_FAQ)
                                .withName(R.string.faq)
                                .withIcon(AppCompatResources.getDrawable(this, R.drawable.ic_action_help_outline))
                                .withIconTintingEnabled(true)
                                .withSelectable(false),
                        PrimaryDrawerItem()
                                .withIdentifier(DRAWER_ABOUT)
                                .withName(R.string.about)
                                .withIcon(AppCompatResources.getDrawable(this, R.drawable.ic_action_copyright))
                                .withIconTintingEnabled(true)
                )
                .withOnDrawerItemClickListener(this)
                .withActionBarDrawerToggle(true)
                .withSavedInstance(savedInstanceState)
        val miniDrawerWidth = resources.getDimension(R.dimen.material_mini_drawer_item)
        if (resources.displayMetrics.widthPixels >=
                resources.getDimension(R.dimen.profile_item_max_width) + miniDrawerWidth) {
            drawer = drawerBuilder.withGenerateMiniDrawer(true).buildView()
            val crossfader = Crossfader<CrossFadeSlidingPaneLayout>()
            this.crossfader = crossfader
            crossfader
                    .withContent(findViewById(android.R.id.content))
                    .withFirst(drawer.slider, resources.getDimensionPixelSize(R.dimen.material_drawer_width))
                    .withSecond(drawer.miniDrawer.build(this), miniDrawerWidth.toInt())
                    .withSavedInstance(savedInstanceState)
                    .build()
            if (resources.configuration.layoutDirection == View.LAYOUT_DIRECTION_RTL)
                crossfader.crossFadeSlidingPaneLayout.setShadowDrawableRight(
                        AppCompatResources.getDrawable(this, R.drawable.material_drawer_shadow_right))
            else crossfader.crossFadeSlidingPaneLayout.setShadowDrawableLeft(
                    AppCompatResources.getDrawable(this, R.drawable.material_drawer_shadow_left))
            drawer.miniDrawer.withCrossFader(object : ICrossfader { // a wrapper is needed
                override fun isCrossfaded(): Boolean = crossfader.isCrossFaded
                override fun crossfade() = crossfader.crossFade()
            })
        } else drawer = drawerBuilder.build()

        if (savedInstanceState == null) displayFragment(ProfilesFragment())
        previousSelectedDrawer = drawer.currentSelection
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
                thread { testConnection(id) }
            }
        }

        fab = findViewById(R.id.fab)
        fab.setOnClickListener {
            if (state == BaseService.CONNECTED) app.stopService() else thread {
                if (BaseService.usingVpnMode) {
                    val intent = VpnService.prepare(this)
                    if (intent != null) startActivityForResult(intent, REQUEST_CONNECT)
                    else app.handler.post { onActivityResult(REQUEST_CONNECT, Activity.RESULT_OK, null) }
                } else app.startService()
            }
        }

        changeState(BaseService.IDLE)   // reset everything to init state
        app.handler.post { connection.connect() }
        DataStore.publicStore.registerChangeListener(this)

        val intent = this.intent
        if (intent != null) handleShareIntent(intent)
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
                .setPositiveButton(R.string.yes, { _, _ -> profiles.forEach { ProfileManager.createProfile(it) } })
                .setNegativeButton(R.string.no, null)
                .setMessage(profiles.joinToString("\n"))
                .create()
                .show()
    }

    override fun onPreferenceDataStoreChanged(store: PreferenceDataStore, key: String?) {
        if (key == Key.serviceMode) app.handler.post {
            connection.disconnect()
            connection.connect()
        }
    }

    private fun displayFragment(fragment: ToolbarFragment) {
        supportFragmentManager.beginTransaction().replace(R.id.fragment_holder, fragment).commitAllowingStateLoss()
        drawer.closeDrawer()
    }

    override fun onItemClick(view: View?, position: Int, drawerItem: IDrawerItem<*, *>): Boolean {
        val id = drawerItem.identifier
        if (id == previousSelectedDrawer) drawer.closeDrawer() else {
            previousSelectedDrawer = id
            when (id) {
                DRAWER_PROFILES -> displayFragment(ProfilesFragment())
                DRAWER_GLOBAL_SETTINGS -> displayFragment(GlobalSettingsFragment())
                DRAWER_ABOUT -> {
                    app.track(TAG, "about")
                    displayFragment(AboutFragment())
                }
                DRAWER_FAQ -> launchUrl(getString(R.string.faq_url))
                DRAWER_CUSTOM_RULES -> displayFragment(CustomRulesFragment())
                else -> return false
            }
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
        if (drawer.isDrawerOpen) drawer.closeDrawer() else {
            val currentFragment = supportFragmentManager.findFragmentById(R.id.fragment_holder) as ToolbarFragment
            if (!currentFragment.onBackPressed())
                if (currentFragment is ProfilesFragment) super.onBackPressed()
                else drawer.setSelection(DRAWER_PROFILES)
        }
    }

    override fun onStop() {
        connection.listeningForBandwidth = false
        super.onStop()
    }

    override fun onSaveInstanceState(outState: Bundle?) {
        super.onSaveInstanceState(outState)
        drawer.saveInstanceState(outState)
        crossfader?.saveInstanceState(outState)
    }

    override fun onDestroy() {
        super.onDestroy()
        DataStore.publicStore.unregisterChangeListener(this)
        connection.disconnect()
        BackupManager(this).dataChanged()
        app.handler.removeCallbacksAndMessages(null)
    }
}
