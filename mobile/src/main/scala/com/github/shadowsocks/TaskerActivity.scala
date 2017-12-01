/*******************************************************************************/
/*                                                                             */
/*  Copyright (C) 2017 by Max Lv <max.c.lv@gmail.com>                          */
/*  Copyright (C) 2017 by Mygod Studio <contact-shadowsocks-android@mygod.be>  */
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

import android.app.Activity
import android.content.res.Resources
import android.os.{Build, Bundle}
import android.support.v7.app.AppCompatActivity
import android.support.v7.widget.{DefaultItemAnimator, LinearLayoutManager, RecyclerView, Toolbar}
import android.view.{LayoutInflater, View, ViewGroup}
import android.widget.{CheckedTextView, Switch}
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.utils.TaskerSettings
import com.github.shadowsocks.ShadowsocksApplication.app

/**
  * @author CzBiX
  */
class TaskerActivity extends AppCompatActivity {
  private class ProfileViewHolder(view: View) extends RecyclerView.ViewHolder(view) with View.OnClickListener {
    {
      val typedArray = obtainStyledAttributes(Array(android.R.attr.selectableItemBackground))
      view.setBackgroundResource(typedArray.getResourceId(0, 0))
      typedArray.recycle()
    }
    private var item: Profile = _
    private val text = itemView.findViewById[CheckedTextView](android.R.id.text1)
    itemView.setOnClickListener(this)

    def bindDefault() {
      item = null
      text.setText(R.string.profile_default)
      text.setChecked(taskerOption.profileId < 0)
    }
    def bind(item: Profile) {
      this.item = item
      text.setText(item.getName)
      text.setChecked(taskerOption.profileId == item.id)
    }

    def onClick(v: View) {
      taskerOption.switchOn = switch.isChecked
      taskerOption.profileId = if (item == null) -1 else item.id
      setResult(Activity.RESULT_OK, taskerOption.toIntent(TaskerActivity.this))
      finish()
    }
  }

  private class ProfilesAdapter extends RecyclerView.Adapter[ProfileViewHolder] {
    val profiles: List[Profile] = app.profileManager.getAllProfiles.getOrElse(List.empty[Profile])
    def getItemCount: Int = 1 + profiles.length
    def onBindViewHolder(vh: ProfileViewHolder, i: Int): Unit = i match {
      case 0 => vh.bindDefault()
      case _ => vh.bind(profiles(i - 1))
    }
    private val name = "select_dialog_singlechoice_" + (if (Build.VERSION.SDK_INT >= 21) "material" else "holo")
    def onCreateViewHolder(vg: ViewGroup, i: Int) = new ProfileViewHolder(LayoutInflater.from(vg.getContext)
      .inflate(Resources.getSystem.getIdentifier(name, "layout", "android"), vg, false))
  }

  private var taskerOption: TaskerSettings = _
  private var switch: Switch = _
  private val profilesAdapter = new ProfilesAdapter

  override def onCreate(savedInstanceState: Bundle) {
    super.onCreate(savedInstanceState)
    try taskerOption = TaskerSettings.fromIntent(getIntent) catch {
      case _: Exception =>
        finish()
        return
    }
    setContentView(R.layout.layout_tasker)

    val toolbar = findViewById(R.id.toolbar).asInstanceOf[Toolbar]
    toolbar.setTitle(R.string.app_name)
    toolbar.setNavigationIcon(R.drawable.ic_navigation_close)
    toolbar.setNavigationOnClickListener(_ => finish())

    switch = findViewById(R.id.serviceSwitch).asInstanceOf[Switch]
    switch.setChecked(taskerOption.switchOn)
    val profilesList = findViewById(R.id.list).asInstanceOf[RecyclerView]
    val lm = new LinearLayoutManager(this, LinearLayoutManager.VERTICAL, false)
    profilesList.setLayoutManager(lm)
    profilesList.setItemAnimator(new DefaultItemAnimator)
    profilesList.setAdapter(profilesAdapter)
    if (taskerOption.profileId >= 0) lm.scrollToPosition(profilesAdapter.profiles.zipWithIndex.collectFirst {
      case (profile, i) if profile.id == taskerOption.profileId => i + 1
    }.getOrElse(0))
  }
}

