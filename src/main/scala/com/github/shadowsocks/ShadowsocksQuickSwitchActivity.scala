/*******************************************************************************/
/*                                                                             */
/*  Copyright (C) 2016 by Max Lv <max.c.lv@gmail.com>                          */
/*  Copyright (C) 2016 by Mygod Studio <contact-shadowsocks-android@mygod.be>  */
/*                                                                             */
/*  This program is free software: you can redistribute it and/or modify       */
/*  it under the terms of the GNU General Public License as published by       */
/*  the Free Software Foundation, either version 3 of the License, or          */
/*  (at your option) any later version.                                        */
/*                                                                             */
/*  This program is distributed in the hope that it will be useful,            */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of             */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              */
/*  GNU General Public License for more details.                               */
/*                                                                             */
/*  You should have received a copy of the GNU General Public License          */
/*  along with this program. If not, see <http://www.gnu.org/licenses/>.       */
/*                                                                             */
/*******************************************************************************/

package com.github.shadowsocks

import android.annotation.SuppressLint
import android.content.pm.ShortcutManager
import android.content.res.Resources
import android.os.{Build, Bundle}
import android.support.v7.app.AppCompatActivity
import android.support.v7.widget.{DefaultItemAnimator, LinearLayoutManager, RecyclerView, Toolbar}
import android.view.{LayoutInflater, View, ViewGroup}
import android.widget.CheckedTextView
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.utils.Utils
import com.github.shadowsocks.ShadowsocksApplication.app

/**
  * Created by Lucas on 3/10/16.
  */
class ShadowsocksQuickSwitchActivity extends AppCompatActivity {

  private class ProfileViewHolder(val view: View) extends RecyclerView.ViewHolder(view) with View.OnClickListener {
    {
      val typedArray = obtainStyledAttributes(Array(android.R.attr.selectableItemBackground))
      view.setBackgroundResource(typedArray.getResourceId(0, 0))
      typedArray.recycle
    }
    private var item: Profile = _
    private val text = itemView.findViewById(android.R.id.text1).asInstanceOf[CheckedTextView]
    itemView.setOnClickListener(this)

    def bind(item: Profile) {
      this.item = item
      text.setText(item.name)
      text.setChecked(item.id == app.profileId)
    }

    def onClick(v: View) {
      app.switchProfile(item.id)
      Utils.startSsService(ShadowsocksQuickSwitchActivity.this)
      finish
    }
  }

  private class ProfilesAdapter extends RecyclerView.Adapter[ProfileViewHolder] {
    val profiles = app.profileManager.getAllProfiles.getOrElse(List.empty[Profile])

    def getItemCount = profiles.length

    def onBindViewHolder(vh: ProfileViewHolder, i: Int) = i match {
      case _ => vh.bind(profiles(i))
    }

    private val name = "select_dialog_singlechoice_" + (if (Build.VERSION.SDK_INT >= 21) "material" else "holo")

    def onCreateViewHolder(vg: ViewGroup, i: Int) = new ProfileViewHolder(LayoutInflater.from(vg.getContext)
      .inflate(Resources.getSystem.getIdentifier(name, "layout", "android"), vg, false))
  }

  private val profilesAdapter = new ProfilesAdapter

  @SuppressLint(Array("NewApi"))
  override def onCreate(savedInstanceState: Bundle) {
    super.onCreate(savedInstanceState)
    setContentView(R.layout.layout_quick_switch)

    val toolbar = findViewById(R.id.toolbar).asInstanceOf[Toolbar]
    toolbar.setTitle(R.string.quick_switch)

    val profilesList = findViewById(R.id.profilesList).asInstanceOf[RecyclerView]
    val lm = new LinearLayoutManager(this)
    profilesList.setLayoutManager(lm)
    profilesList.setItemAnimator(new DefaultItemAnimator)
    profilesList.setAdapter(profilesAdapter)
    if (app.profileId >= 0) lm.scrollToPosition(profilesAdapter.profiles.zipWithIndex.collectFirst {
      case (profile, i) if profile.id == app.profileId => i
    }.getOrElse(0))
    if (Build.VERSION.SDK_INT >= 25) getSystemService(classOf[ShortcutManager]).reportShortcutUsed("switch")
  }
}
