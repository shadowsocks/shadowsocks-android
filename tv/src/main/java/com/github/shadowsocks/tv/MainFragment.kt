/*******************************************************************************
 *                                                                             *
 *  Copyright (C) 2018 by Max Lv <max.c.lv@gmail.com>                          *
 *  Copyright (C) 2018 by Mygod Studio <contact-shadowsocks-android@mygod.be>  *
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

package com.github.shadowsocks.tv

import android.os.Bundle
import android.view.View
import android.view.ViewGroup
import androidx.core.os.bundleOf
import androidx.core.view.updateLayoutParams
import androidx.leanback.preference.LeanbackPreferenceDialogFragmentCompat
import androidx.leanback.preference.LeanbackSettingsFragmentCompat
import androidx.preference.*
import com.github.shadowsocks.bg.BaseService
import com.github.shadowsocks.tv.preference.LeanbackSingleListPreferenceDialogFragment
import com.github.shadowsocks.utils.Key

class MainFragment : LeanbackSettingsFragmentCompat() {
    override fun onPreferenceStartInitialScreen() = startPreferenceFragment(MainPreferenceFragment())
    override fun onPreferenceStartScreen(caller: PreferenceFragmentCompat?, pref: PreferenceScreen?): Boolean {
        onPreferenceStartInitialScreen()
        return true
    }
    override fun onPreferenceStartFragment(caller: PreferenceFragmentCompat?, pref: Preference?) = false
    override fun onPreferenceDisplayDialog(caller: PreferenceFragmentCompat, pref: Preference?): Boolean {
        if (pref?.key == Key.id) {
            if ((childFragmentManager.findFragmentById(R.id.settings_preference_fragment_container)
                            as MainPreferenceFragment).state == BaseService.State.Stopped) {
                startPreferenceFragment(ProfilesDialogFragment().apply {
                    arguments = bundleOf(Pair(LeanbackPreferenceDialogFragmentCompat.ARG_KEY, Key.id))
                    setTargetFragment(caller, 0)
                })
            }
            return true
        }
        if (pref is ListPreference && pref !is MultiSelectListPreference) {
            startPreferenceFragment(LeanbackSingleListPreferenceDialogFragment().apply {
                arguments = bundleOf(Pair(LeanbackPreferenceDialogFragmentCompat.ARG_KEY, pref.key))
                setTargetFragment(caller, 0)
            })
            return true
        }
        return super.onPreferenceDisplayDialog(caller, pref)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        view.findViewById<View>(R.id.settings_preference_fragment_container).updateLayoutParams {
            width = ViewGroup.LayoutParams.MATCH_PARENT
        }
    }
}
