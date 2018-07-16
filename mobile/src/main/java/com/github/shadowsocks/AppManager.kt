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

import android.Manifest
import android.animation.Animator
import android.animation.AnimatorListenerAdapter
import android.annotation.SuppressLint
import android.content.BroadcastReceiver
import android.content.ClipData
import android.content.ClipboardManager
import android.content.pm.ApplicationInfo
import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import android.graphics.drawable.Drawable
import android.os.Bundle
import android.os.Handler
import com.google.android.material.snackbar.Snackbar
import androidx.core.app.TaskStackBuilder
import androidx.appcompat.app.AppCompatActivity
import androidx.recyclerview.widget.DefaultItemAnimator
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import androidx.appcompat.widget.Toolbar
import android.view.*
import android.widget.ImageView
import android.widget.Switch
import androidx.core.content.getSystemService
import com.github.shadowsocks.App.Companion.app
import com.github.shadowsocks.database.ProfileManager
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.utils.*
import java.util.concurrent.atomic.AtomicBoolean

class AppManager : AppCompatActivity(), Toolbar.OnMenuItemClickListener {
    companion object {
        @SuppressLint("StaticFieldLeak")
        private var instance: AppManager? = null

        private var receiver: BroadcastReceiver? = null
        private var cachedApps: List<PackageInfo>? = null
        private fun getApps(pm: PackageManager) = synchronized(AppManager) {
            if (receiver == null) receiver = app.listenForPackageChanges {
                synchronized(AppManager) {
                    receiver = null
                    cachedApps = null
                }
                AppManager.instance?.reloadApps()
            }
            // Labels and icons can change on configuration (locale, etc.) changes, therefore they are not cached.
            val cachedApps = cachedApps ?: pm.getInstalledPackages(PackageManager.GET_PERMISSIONS)
                    .filter { it.packageName != app.packageName &&
                            it.requestedPermissions?.contains(Manifest.permission.INTERNET) ?: false }
            this.cachedApps = cachedApps
            cachedApps
        }.map { ProxiedApp(pm, it.applicationInfo, it.packageName) }
    }

    private class ProxiedApp(private val pm: PackageManager, private val appInfo: ApplicationInfo,
                             val packageName: String) {
        val name: CharSequence = appInfo.loadLabel(pm)    // cached for sorting
        val icon: Drawable get() = appInfo.loadIcon(pm)
    }

    private inner class AppViewHolder(view: View) : RecyclerView.ViewHolder(view), View.OnClickListener {
        private val icon = view.findViewById<ImageView>(R.id.itemicon)
        private val check = view.findViewById<Switch>(R.id.itemcheck)
        private lateinit var item: ProxiedApp
        private val proxied get() = proxiedApps.contains(item.packageName)

        init {
            view.setOnClickListener(this)
        }

        fun bind(app: ProxiedApp) {
            this.item = app
            icon.setImageDrawable(app.icon)
            check.text = app.name
            check.isChecked = proxied
        }

        override fun onClick(v: View?) {
            if (proxied) {
                proxiedApps.remove(item.packageName)
                check.isChecked = false
            } else {
                proxiedApps.add(item.packageName)
                check.isChecked = true
            }
            if (!appsLoading.get()) {
                DataStore.individual = proxiedApps.joinToString("\n")
                DataStore.dirty = true
            }
        }
    }

    private inner class AppsAdapter : RecyclerView.Adapter<AppViewHolder>() {
        private var apps = listOf<ProxiedApp>()

        fun reload() {
            apps = getApps(packageManager)
                    .sortedWith(compareBy({ !proxiedApps.contains(it.packageName) }, { it.name.toString() }))
        }

        override fun onBindViewHolder(holder: AppViewHolder, position: Int) = holder.bind(apps[position])
        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): AppViewHolder =
                AppViewHolder(LayoutInflater.from(parent.context).inflate(R.layout.layout_apps_item, parent, false))
        override fun getItemCount(): Int = apps.size
    }

    private lateinit var proxiedApps: HashSet<String>
    private lateinit var toolbar: Toolbar
    private lateinit var bypassSwitch: Switch
    private lateinit var appListView: RecyclerView
    private lateinit var loadingView: View
    private val appsLoading = AtomicBoolean()
    private val handler = Handler()
    private val clipboard by lazy { getSystemService<ClipboardManager>()!! }

    private fun initProxiedApps(str: String = DataStore.individual) {
        proxiedApps = str.split('\n').toHashSet()
    }
    private fun reloadApps() {
        if (!appsLoading.compareAndSet(true, false)) loadAppsAsync()
    }
    private fun loadAppsAsync() {
        if (!appsLoading.compareAndSet(false, true)) return
        appListView.visibility = View.GONE
        loadingView.visibility = View.VISIBLE
        thread("AppManager-loader") {
            val adapter = appListView.adapter as AppsAdapter
            do {
                appsLoading.set(true)
                adapter.reload()
            } while (!appsLoading.compareAndSet(true, false))
            handler.post {
                adapter.notifyDataSetChanged()
                val shortAnimTime = resources.getInteger(android.R.integer.config_shortAnimTime)
                appListView.alpha = 0F
                appListView.visibility = View.VISIBLE
                appListView.animate().alpha(1F).duration = shortAnimTime.toLong()
                loadingView.animate().alpha(0F).setListener(object : AnimatorListenerAdapter() {
                    override fun onAnimationEnd(animation: Animator) {
                        loadingView.visibility = View.GONE
                    }
                }).duration = shortAnimTime.toLong()
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.layout_apps)
        toolbar = findViewById(R.id.toolbar)
        toolbar.setTitle(R.string.proxied_apps)
        toolbar.setNavigationIcon(theme.resolveResourceId(R.attr.homeAsUpIndicator))
        toolbar.setNavigationOnClickListener {
            val intent = parentActivityIntent
            if (shouldUpRecreateTask(intent) || isTaskRoot)
                TaskStackBuilder.create(this).addNextIntentWithParentStack(intent).startActivities() else finish()
        }
        toolbar.inflateMenu(R.menu.app_manager_menu)
        toolbar.setOnMenuItemClickListener(this)

        if (!DataStore.proxyApps) {
            DataStore.proxyApps = true
            DataStore.dirty = true
        }
        findViewById<Switch>(R.id.onSwitch).setOnCheckedChangeListener { _, checked ->
            DataStore.proxyApps = checked
            DataStore.dirty = true
            finish()
        }

        bypassSwitch = findViewById(R.id.bypassSwitch)
        bypassSwitch.isChecked = DataStore.bypass
        bypassSwitch.setOnCheckedChangeListener { _, checked ->
            DataStore.bypass = checked
            DataStore.dirty = true
        }

        initProxiedApps()
        loadingView = findViewById(R.id.loading)
        appListView = findViewById(R.id.list)
        appListView.layoutManager = LinearLayoutManager(this, RecyclerView.VERTICAL, false)
        appListView.itemAnimator = DefaultItemAnimator()
        appListView.adapter = AppsAdapter()

        instance = this
        loadAppsAsync()
    }

    override fun onMenuItemClick(item: MenuItem): Boolean {
        when (item.itemId) {
            R.id.action_apply_all -> {
                val profiles = ProfileManager.getAllProfiles()
                if (profiles != null) {
                    val proxiedAppString = DataStore.individual
                    profiles.forEach {
                        it.individual = proxiedAppString
                        ProfileManager.updateProfile(it)
                    }
                    if (DataStore.directBootAware) DirectBoot.update()
                    Snackbar.make(appListView, R.string.action_apply_all, Snackbar.LENGTH_LONG).show()
                } else Snackbar.make(appListView, R.string.action_export_err, Snackbar.LENGTH_LONG).show()
                return true
            }
            R.id.action_export -> {
                clipboard.primaryClip = ClipData.newPlainText(Key.individual,
                        "${DataStore.bypass}\n${DataStore.individual}")
                Snackbar.make(appListView, R.string.action_export_msg, Snackbar.LENGTH_LONG).show()
                return true
            }
            R.id.action_import -> {
                val proxiedAppString = clipboard.primaryClip?.getItemAt(0)?.text?.toString()
                if (!proxiedAppString.isNullOrEmpty()) {
                    val i = proxiedAppString!!.indexOf('\n')
                    try {
                        val (enabled, apps) = if (i < 0) Pair(proxiedAppString, "") else
                            Pair(proxiedAppString.substring(0, i), proxiedAppString.substring(i + 1))
                        bypassSwitch.isChecked = enabled.toBoolean()
                        DataStore.individual = apps
                        DataStore.dirty = true
                        Snackbar.make(appListView, R.string.action_import_msg, Snackbar.LENGTH_LONG).show()
                        initProxiedApps(apps)
                        reloadApps()
                        return true
                    } catch (_: IllegalArgumentException) { }
                }
                Snackbar.make(appListView, R.string.action_import_err, Snackbar.LENGTH_LONG).show()
            }
        }
        return false
    }

    override fun onKeyUp(keyCode: Int, event: KeyEvent?) = if (keyCode == KeyEvent.KEYCODE_MENU)
        if (toolbar.isOverflowMenuShowing) toolbar.hideOverflowMenu() else toolbar.showOverflowMenu()
    else super.onKeyUp(keyCode, event)

    override fun onDestroy() {
        instance = null
        handler.removeCallbacksAndMessages(null)
        super.onDestroy()
    }
}
