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

import androidx.core.os.bundleOf
import androidx.leanback.preference.LeanbackPreferenceDialogFragment
import androidx.leanback.preference.LeanbackSettingsFragment
import androidx.preference.*
import com.github.shadowsocks.bg.BaseService
import com.github.shadowsocks.tv.preference.LeanbackSingleListPreferenceDialogFragment
import com.github.shadowsocks.utils.Key

class MainFragment : LeanbackSettingsFragment() {
    override fun onPreferenceStartInitialScreen() = startPreferenceFragment(MainPreferenceFragment())
    override fun onPreferenceStartScreen(caller: PreferenceFragment?, pref: PreferenceScreen?): Boolean {
        onPreferenceStartInitialScreen()
        return true
    }
    override fun onPreferenceStartFragment(caller: PreferenceFragment?, pref: Preference?) = false
    override fun onPreferenceDisplayDialog(caller: PreferenceFragment, pref: Preference?): Boolean {
        if (pref?.key == Key.id) {
            if ((childFragmentManager.findFragmentById(R.id.settings_preference_fragment_container)
                            as MainPreferenceFragment).state == BaseService.STOPPED) {
                startPreferenceFragment(ProfilesDialogFragment().apply {
                    arguments = bundleOf(Pair(LeanbackPreferenceDialogFragment.ARG_KEY, Key.id))
                    setTargetFragment(caller, 0)
                })
            }
            return true
        }
        if (pref is ListPreference && pref !is MultiSelectListPreference) {
            startPreferenceFragment(LeanbackSingleListPreferenceDialogFragment().apply {
                arguments = bundleOf(Pair(LeanbackPreferenceDialogFragment.ARG_KEY, pref.key))
                setTargetFragment(caller, 0)
            })
            return true
        }
        return super.onPreferenceDisplayDialog(caller, pref)
    }
}
