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
import android.content.Intent
import android.content.pm.ApplicationInfo
import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import android.graphics.drawable.Drawable
import android.os.Bundle
import android.util.SparseBooleanArray
import android.view.*
import android.widget.Filter
import android.widget.Filterable
import android.widget.SearchView
import androidx.annotation.UiThread
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.getSystemService
import androidx.core.util.set
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.DefaultItemAnimator
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.github.shadowsocks.Core.app
import com.github.shadowsocks.database.ProfileManager
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.utils.DirectBoot
import com.github.shadowsocks.utils.Key
import com.github.shadowsocks.utils.SingleInstanceActivity
import com.github.shadowsocks.utils.listenForPackageChanges
import com.github.shadowsocks.widget.ListHolderListener
import com.github.shadowsocks.widget.ListListener
import com.google.android.material.snackbar.Snackbar
import kotlinx.android.synthetic.main.layout_apps.*
import kotlinx.android.synthetic.main.layout_apps_item.view.*
import kotlinx.coroutines.*
import me.zhanghai.android.fastscroll.FastScrollerBuilder
import me.zhanghai.android.fastscroll.PopupTextProvider
import kotlin.coroutines.coroutineContext

class AppManager : AppCompatActivity() {
    companion object {
        @SuppressLint("StaticFieldLeak")
        private var instance: AppManager? = null
        private const val SWITCH = "switch"

        private var receiver: BroadcastReceiver? = null
        private var cachedApps: Map<String, PackageInfo>? = null
        private fun getCachedApps(pm: PackageManager) = synchronized(AppManager) {
            if (receiver == null) receiver = app.listenForPackageChanges {
                synchronized(AppManager) {
                    receiver = null
                    cachedApps = null
                }
                instance?.loadApps()
            }
            // Labels and icons can change on configuration (locale, etc.) changes, therefore they are not cached.
            val cachedApps = cachedApps ?: pm.getInstalledPackages(
                    PackageManager.GET_PERMISSIONS or PackageManager.MATCH_UNINSTALLED_PACKAGES)
                    .filter {
                        when (it.packageName) {
                            app.packageName -> false
                            "android" -> true
                            else -> it.requestedPermissions?.contains(Manifest.permission.INTERNET) == true
                        }
                    }
                    .associateBy { it.packageName }
            this.cachedApps = cachedApps
            cachedApps
        }
    }

    private class ProxiedApp(private val pm: PackageManager, private val appInfo: ApplicationInfo,
                             val packageName: String) {
        val name: CharSequence = appInfo.loadLabel(pm)    // cached for sorting
        val icon: Drawable get() = appInfo.loadIcon(pm)
        val uid get() = appInfo.uid
    }

    private inner class AppViewHolder(view: View) : RecyclerView.ViewHolder(view), View.OnClickListener {
        private lateinit var item: ProxiedApp

        init {
            view.setOnClickListener(this)
        }

        fun bind(app: ProxiedApp) {
            item = app
            itemView.itemicon.setImageDrawable(app.icon)
            itemView.title.text = app.name
            itemView.desc.text = "${app.packageName} (${app.uid})"
            itemView.itemcheck.isChecked = isProxiedApp(app)
        }

        fun handlePayload(payloads: List<String>) {
            if (payloads.contains(SWITCH)) itemView.itemcheck.isChecked = isProxiedApp(item)
        }

        override fun onClick(v: View?) {
            if (isProxiedApp(item)) proxiedUids.delete(item.uid) else proxiedUids[item.uid] = true
            DataStore.individual = apps.filter { isProxiedApp(it) }.joinToString("\n") { it.packageName }
            DataStore.dirty = true

            appsAdapter.notifyItemRangeChanged(0, appsAdapter.itemCount, SWITCH)
        }
    }

    private inner class AppsAdapter : RecyclerView.Adapter<AppViewHolder>(), Filterable, PopupTextProvider {
        private var filteredApps = apps

        suspend fun reload() {
            apps = getCachedApps(packageManager).map { (packageName, packageInfo) ->
                coroutineContext[Job]!!.ensureActive()
                ProxiedApp(packageManager, packageInfo.applicationInfo, packageName)
            }.sortedWith(compareBy({ !isProxiedApp(it) }, { it.name.toString() }))
        }

        override fun onBindViewHolder(holder: AppViewHolder, position: Int) = holder.bind(filteredApps[position])
        override fun onBindViewHolder(holder: AppViewHolder, position: Int, payloads: List<Any>) {
            if (payloads.isNotEmpty()) {
                @Suppress("UNCHECKED_CAST")
                holder.handlePayload(payloads as List<String>)
                return
            }

            onBindViewHolder(holder, position)
        }
        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): AppViewHolder =
                AppViewHolder(LayoutInflater.from(parent.context).inflate(R.layout.layout_apps_item, parent, false))
        override fun getItemCount(): Int = filteredApps.size

        private val filterImpl = object : Filter() {
            override fun performFiltering(constraint: CharSequence) = FilterResults().apply {
                val filteredApps = if (constraint.isEmpty()) apps else apps.filter {
                    it.name.contains(constraint, true) ||
                            it.packageName.contains(constraint, true) ||
                            it.uid.toString().contains(constraint)
                }
                count = filteredApps.size
                values = filteredApps
            }

            override fun publishResults(constraint: CharSequence, results: FilterResults) {
                @Suppress("UNCHECKED_CAST")
                filteredApps = results.values as List<ProxiedApp>
                notifyDataSetChanged()
            }
        }
        override fun getFilter(): Filter = filterImpl

        override fun getPopupText(position: Int) = filteredApps[position].name.firstOrNull()?.toString() ?: ""
    }

    private val proxiedUids = SparseBooleanArray()
    private val clipboard by lazy { getSystemService<ClipboardManager>()!! }
    private var loader: Job? = null
    private var apps = emptyList<ProxiedApp>()
    private val appsAdapter = AppsAdapter()

    private val shortAnimTime by lazy { resources.getInteger(android.R.integer.config_shortAnimTime).toLong() }
    private fun View.crossFadeFrom(other: View) {
        clearAnimation()
        other.clearAnimation()
        if (visibility == View.VISIBLE && other.visibility == View.GONE) return
        alpha = 0F
        visibility = View.VISIBLE
        animate().alpha(1F).duration = shortAnimTime
        other.animate().alpha(0F).setListener(object : AnimatorListenerAdapter() {
            override fun onAnimationEnd(animation: Animator) {
                other.visibility = View.GONE
            }
        }).duration = shortAnimTime
    }

    private fun initProxiedUids(str: String = DataStore.individual) {
        proxiedUids.clear()
        val apps = getCachedApps(packageManager)
        for (line in str.lineSequence()) proxiedUids[(apps[line] ?: continue).applicationInfo.uid] = true
    }

    private fun isProxiedApp(app: ProxiedApp) = proxiedUids[app.uid]

    @UiThread
    private fun loadApps() {
        loader?.cancel()
        loader = lifecycleScope.launchWhenCreated {
            loading.crossFadeFrom(list)
            val adapter = list.adapter as AppsAdapter
            withContext(Dispatchers.IO) { adapter.reload() }
            adapter.filter.filter(search.query)
            list.crossFadeFrom(loading)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        SingleInstanceActivity.register(this) ?: return
        setContentView(R.layout.layout_apps)
        ListHolderListener.setup(this)
        setSupportActionBar(toolbar)
        supportActionBar!!.setDisplayHomeAsUpEnabled(true)

        if (!DataStore.proxyApps) {
            DataStore.proxyApps = true
            DataStore.dirty = true
        }

        bypassGroup.check(if (DataStore.bypass) R.id.btn_bypass else R.id.btn_on)
        bypassGroup.setOnCheckedChangeListener { _, checkedId ->
            DataStore.dirty = true
            when (checkedId) {
                R.id.btn_off -> {
                    DataStore.proxyApps = false
                    finish()
                }
                R.id.btn_on -> DataStore.bypass = false
                R.id.btn_bypass -> DataStore.bypass = true
            }
        }

        initProxiedUids()
        list.setOnApplyWindowInsetsListener(ListListener)
        list.layoutManager = LinearLayoutManager(this, RecyclerView.VERTICAL, false)
        list.itemAnimator = DefaultItemAnimator()
        list.adapter = appsAdapter
        FastScrollerBuilder(list).useMd2Style().build()

        search.setOnQueryTextListener(object : SearchView.OnQueryTextListener {
            override fun onQueryTextSubmit(query: String?) = false
            override fun onQueryTextChange(newText: String?) = true.also { appsAdapter.filter.filter(newText) }
        })

        instance = this
        loadApps()
    }

    override fun onCreateOptionsMenu(menu: Menu?): Boolean {
        menuInflater.inflate(R.menu.app_manager_menu, menu)
        return true
    }
    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        when (item.itemId) {
            R.id.action_apply_all -> {
                val profiles = ProfileManager.getAllProfiles()
                if (profiles != null) {
                    val proxiedAppString = DataStore.individual
                    profiles.forEach {
                        it.individual = proxiedAppString
                        it.bypass = DataStore.bypass
                        ProfileManager.updateProfile(it)
                    }
                    if (DataStore.directBootAware) DirectBoot.update()
                    Snackbar.make(list, R.string.action_apply_all, Snackbar.LENGTH_LONG).show()
                } else Snackbar.make(list, R.string.action_export_err, Snackbar.LENGTH_LONG).show()
                return true
            }
            R.id.action_export_clipboard -> {
                clipboard.setPrimaryClip(ClipData.newPlainText(Key.individual,
                        "${DataStore.bypass}\n${DataStore.individual}"))
                Snackbar.make(list, R.string.action_export_msg, Snackbar.LENGTH_LONG).show()
                return true
            }
            R.id.action_import_clipboard -> {
                val proxiedAppString = clipboard.primaryClip?.getItemAt(0)?.text?.toString()
                if (!proxiedAppString.isNullOrEmpty()) {
                    val i = proxiedAppString.indexOf('\n')
                    try {
                        val (enabled, apps) = if (i < 0) Pair(proxiedAppString, "") else
                            Pair(proxiedAppString.substring(0, i), proxiedAppString.substring(i + 1))
                        bypassGroup.check(if (enabled.toBoolean()) R.id.btn_bypass else R.id.btn_on)
                        DataStore.individual = apps
                        DataStore.dirty = true
                        Snackbar.make(list, R.string.action_import_msg, Snackbar.LENGTH_LONG).show()
                        initProxiedUids(apps)
                        appsAdapter.notifyItemRangeChanged(0, appsAdapter.itemCount, SWITCH)
                        return true
                    } catch (_: IllegalArgumentException) { }
                }
                Snackbar.make(list, R.string.action_import_err, Snackbar.LENGTH_LONG).show()
            }
        }
        return super.onOptionsItemSelected(item)
    }

    override fun supportNavigateUpTo(upIntent: Intent) =
            super.supportNavigateUpTo(upIntent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP))

    override fun onKeyUp(keyCode: Int, event: KeyEvent?) = if (keyCode == KeyEvent.KEYCODE_MENU)
        if (toolbar.isOverflowMenuShowing) toolbar.hideOverflowMenu() else toolbar.showOverflowMenu()
    else super.onKeyUp(keyCode, event)

    override fun onDestroy() {
        instance = null
        loader?.cancel()
        super.onDestroy()
    }
}
