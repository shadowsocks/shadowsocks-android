/*******************************************************************************
 *                                                                             *
 *  Copyright (C) 2019 by Max Lv <max.c.lv@gmail.com>                          *
 *  Copyright (C) 2019 by Mygod Studio <contact-shadowsocks-android@mygod.be>  *
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

import android.content.DialogInterface
import android.os.Bundle
import android.os.Parcelable
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import com.github.shadowsocks.core.R
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.database.ProfileManager
import com.github.shadowsocks.plugin.fragment.AlertDialogFragment
import com.github.shadowsocks.plugin.fragment.Empty
import com.github.shadowsocks.plugin.fragment.showAllowingStateLoss
import com.github.shadowsocks.utils.SubscriptionUrls
import com.github.shadowsocks.utils.readableMessage
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.parcelize.Parcelize
import timber.log.Timber
import java.net.HttpURLConnection

class UrlImportActivity : AppCompatActivity() {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main)

    @Parcelize
    data class ProfilesArg(val profiles: List<Profile>) : Parcelable
    class ImportProfilesDialogFragment : AlertDialogFragment<ProfilesArg, Empty>() {
        override fun AlertDialog.Builder.prepare(listener: DialogInterface.OnClickListener) {
            setTitle(R.string.add_profile_dialog)
            setPositiveButton(R.string.yes, listener)
            setNegativeButton(R.string.no, listener)
            setMessage(arg.profiles.joinToString("\n"))
        }

        override fun onClick(dialog: DialogInterface?, which: Int) {
            if (which == DialogInterface.BUTTON_POSITIVE) arg.profiles.forEach { ProfileManager.createProfile(it) }
            requireActivity().finish()
        }

        override fun onDismiss(dialog: DialogInterface) {
            super.onDismiss(dialog)
            requireActivity().finish()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        scope.launch {
            when (val dialog = handleShareIntent()) {
                null -> {
                    Toast.makeText(this@UrlImportActivity, R.string.profile_invalid_input, Toast.LENGTH_SHORT).show()
                    finish()
                }
                else -> dialog.showAllowingStateLoss(supportFragmentManager)
            }
        }
    }

    private suspend fun handleShareIntent() = intent.data?.let { data ->
        when (data.scheme?.lowercase()) {
            "ssconf" -> handleSsConfIntent(data.toString())
            else -> handleSsUri(data.toString())
        }
    }

    private fun handleSsUri(sharedStr: String): ImportProfilesDialogFragment? {
        val profiles = Profile.findAllUrls(sharedStr, Core.currentProfile?.main).toList()
        return if (profiles.isEmpty()) null else ImportProfilesDialogFragment().apply {
            arg(ProfilesArg(profiles))
            key()
        }
    }

    private suspend fun handleSsConfIntent(sharedStr: String) = try {
        val json = withContext(Dispatchers.IO) {
            (SubscriptionUrls.parse(sharedStr).openConnection() as HttpURLConnection).run {
                inputStream.bufferedReader().use { it.readText() }
            }
        }
        val profiles = mutableListOf<Profile>()
        Profile.parseJson(json, Core.currentProfile?.main) {
            it.also(profiles::add)
        }
        if (profiles.isEmpty()) null else ImportProfilesDialogFragment().apply {
            arg(ProfilesArg(profiles))
            key()
        }
    } catch (e: Exception) {
        Timber.w(e)
        Toast.makeText(this, e.readableMessage, Toast.LENGTH_LONG).show()
        null
    }

    override fun onDestroy() {
        scope.cancel()
        super.onDestroy()
    }
}
