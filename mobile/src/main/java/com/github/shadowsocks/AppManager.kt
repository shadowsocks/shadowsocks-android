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
import android.content.Context
import android.content.pm.PackageManager
import android.graphics.drawable.Drawable
import android.os.Bundle
import android.os.Handler
import android.support.v4.app.TaskStackBuilder
import android.support.v7.app.AppCompatActivity
import android.support.v7.widget.DefaultItemAnimator
import android.support.v7.widget.LinearLayoutManager
import android.support.v7.widget.RecyclerView
import android.support.v7.widget.Toolbar
import android.view.*
import android.widget.ImageView
import android.widget.Switch
import android.widget.Toast
import com.futuremind.recyclerviewfastscroll.FastScroller
import com.futuremind.recyclerviewfastscroll.SectionTitleProvider
import com.github.shadowsocks.App.Companion.app
import com.github.shadowsocks.database.ProfileManager
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.utils.Key
import com.github.shadowsocks.utils.thread
import java.util.concurrent.atomic.AtomicBoolean

class AppManager : AppCompatActivity(), Toolbar.OnMenuItemClickListener {
    companion object {
        data class ProxiedApp(val name: String, val packageName: String, val icon: Drawable)

        @SuppressLint("StaticFieldLeak")
        private var instance: AppManager? = null

        private var receiver: BroadcastReceiver? = null
        private var cachedApps: Array<ProxiedApp>? = null
        private fun getApps(pm: PackageManager): Array<ProxiedApp> {
            if (receiver == null) receiver = app.listenForPackageChanges {
                synchronized(AppManager) { cachedApps = null }
                AppManager.instance?.reloadApps()
            }
            return synchronized(AppManager) {
                val cachedApps = cachedApps ?: pm.getInstalledPackages(PackageManager.GET_PERMISSIONS)
                        .filter { it.requestedPermissions?.contains(Manifest.permission.INTERNET) ?: false }
                        .map { ProxiedApp(pm.getApplicationLabel(it.applicationInfo).toString(), it.packageName,
                                it.applicationInfo.loadIcon(pm)) }
                        .toTypedArray()
                this.cachedApps = cachedApps
                cachedApps
            }
        }
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

    private inner class AppsAdapter : RecyclerView.Adapter<AppViewHolder>(), SectionTitleProvider {
        private val apps = getApps(packageManager).sortedWith(Comparator { a, b ->
            when (Pair(proxiedApps.contains(a.packageName), proxiedApps.contains(b.packageName))) {
                Pair(true, false) -> -1
                Pair(false, true) -> 1
                else -> a.name.compareTo(b.name, true)
            }
        })

        override fun onBindViewHolder(holder: AppViewHolder, position: Int) = holder.bind(apps[position])
        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): AppViewHolder =
                AppViewHolder(LayoutInflater.from(parent.context).inflate(R.layout.layout_apps_item, parent, false))
        override fun getItemCount(): Int = apps.size
        override fun getSectionTitle(position: Int): String = apps[position].name.substring(0, 1)
    }

    private lateinit var proxiedApps: HashSet<String>
    private lateinit var toolbar: Toolbar
    private lateinit var bypassSwitch: Switch
    private lateinit var appListView: RecyclerView
    private lateinit var fastScroller: FastScroller
    private lateinit var loadingView: View
    private val appsLoading = AtomicBoolean()
    private val handler = Handler()
    private val clipboard by lazy { getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager }

    private fun initProxiedApps(str: String = DataStore.individual) {
        proxiedApps = str.split('\n').toHashSet()
    }
    private fun reloadApps() {
        if (!appsLoading.compareAndSet(true, false)) loadAppsAsync()
    }
    private fun loadAppsAsync() {
        if (!appsLoading.compareAndSet(false, true)) return
        thread {
            var adapter: AppsAdapter?
            do {
                appsLoading.set(true)
                adapter = AppsAdapter()
            } while (!appsLoading.compareAndSet(true, false))
            handler.post {
                appListView.adapter = adapter
                fastScroller.setRecyclerView(appListView)
                val shortAnimTime = resources.getInteger(android.R.integer.config_shortAnimTime)
                appListView.alpha = 0F
                appListView.visibility = View.VISIBLE
                appListView.animate().alpha(1F).duration = shortAnimTime.toLong()
                fastScroller.alpha = 0F
                fastScroller.visibility = View.VISIBLE
                fastScroller.animate().alpha(1F).duration = shortAnimTime.toLong()
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
        toolbar.setNavigationIcon(R.drawable.abc_ic_ab_back_material)
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
        appListView.layoutManager = LinearLayoutManager(this, LinearLayoutManager.VERTICAL, false)
        appListView.itemAnimator = DefaultItemAnimator()
        fastScroller = findViewById(R.id.fastscroller)

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
                    Toast.makeText(this, R.string.action_apply_all, Toast.LENGTH_SHORT).show()
                } else Toast.makeText(this, R.string.action_export_err, Toast.LENGTH_SHORT).show()
                return true
            }
            R.id.action_export -> {
                clipboard.primaryClip = ClipData.newPlainText(Key.individual,
                        "${DataStore.bypass}\n${DataStore.individual}")
                Toast.makeText(this, R.string.action_export_msg, Toast.LENGTH_SHORT).show()
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
                        Toast.makeText(this, R.string.action_import_msg, Toast.LENGTH_SHORT).show()
                        appListView.visibility = View.GONE
                        fastScroller.visibility = View.GONE
                        loadingView.visibility = View.VISIBLE
                        initProxiedApps(apps)
                        reloadApps()
                        return true
                    } catch (_: IllegalArgumentException) { }
                }
                Toast.makeText(this, R.string.action_import_err, Toast.LENGTH_SHORT).show()
            }
        }
        return false
    }

    override fun onKeyUp(keyCode: Int, event: KeyEvent?): Boolean {
        return if (keyCode == KeyEvent.KEYCODE_MENU)
            if (toolbar.isOverflowMenuShowing) toolbar.hideOverflowMenu() else toolbar.showOverflowMenu()
        else super.onKeyUp(keyCode, event)
    }

    override fun onDestroy() {
        instance = null
        handler.removeCallbacksAndMessages(null)
        super.onDestroy()
    }
}
