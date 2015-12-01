/*
 * Shadowsocks - A shadowsocks client for Android
 * Copyright (C) 2014 <max.c.lv@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *                            ___====-_  _-====___
 *                      _--^^^#####//      \\#####^^^--_
 *                   _-^##########// (    ) \\##########^-_
 *                  -############//  |\^^/|  \\############-
 *                _/############//   (@::@)   \\############\_
 *               /#############((     \\//     ))#############\
 *              -###############\\    (oo)    //###############-
 *             -#################\\  / VV \  //#################-
 *            -###################\\/      \//###################-
 *           _#/|##########/\######(   /\   )######/\##########|\#_
 *           |/ |#/\#/\#/\/  \#/\##\  |  |  /##/\#/  \/\#/\#/\#| \|
 *           `  |/  V  V  `   V  \#\| |  | |/#/  V   '  V  V  \|  '
 *              `   `  `      `   / | |  | | \   '      '  '   '
 *                               (  | |  | |  )
 *                              __\ | |  | | /__
 *                             (vvv(VVV)(VVV)vvv)
 *
 *                              HERE BE DRAGONS
 *
 */
package com.github.shadowsocks

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import android.support.v4.app.{Fragment, ListFragment}
import android.support.v7.app.AppCompatActivity
import android.support.v7.widget.Toolbar
import android.util.Log
import android.view.{LayoutInflater, View, ViewGroup}
import android.widget.AdapterView.OnItemClickListener
import android.widget._
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.helper.TaskerSettings
import com.twofortyfouram.locale.api.{Intent => ApiIntent}

/**
  * @author CzBiX
  */
class TaskerActivity extends AppCompatActivity {
  var taskerOption: TaskerSettings = _
  var actionSelected: Boolean = false

  override def onCreate(savedInstanceState: Bundle) {
    super.onCreate(savedInstanceState)
    setContentView(R.layout.layout_tasker)

    val toolbar = findViewById(R.id.toolbar).asInstanceOf[Toolbar]
    toolbar.setTitle(R.string.app_name)
    toolbar.setNavigationIcon(R.drawable.ic_close)
    toolbar.setNavigationOnClickListener(_ => finish())
    toolbar.inflateMenu(R.menu.tasker_menu)
    toolbar.setOnMenuItemClickListener(_.getItemId match {
      case R.id.save =>
        saveResult()
        true
      case _ => false
    })

    loadSettings()
    loadFragment()
  }

  private def loadSettings() {
    val intent: Intent = getIntent
    if (intent.getAction != ApiIntent.ACTION_EDIT_SETTING) {
      Log.w(Shadowsocks.TAG, "unknown tasker action")
      finish()
      return
    }

    taskerOption = TaskerSettings.fromIntent(intent)
  }

  private def loadFragment() {
    actionSelected = !taskerOption.isEmpty
    val fragment: Fragment = taskerOption.action match {
      case TaskerSettings.ACTION_TOGGLE_SERVICE =>
        new ToggleServiceFragment
      case TaskerSettings.ACTION_SWITCH_PROFILE =>
        new ProfileChoiceFragment
      case _ =>
        val fragment: ActionChoiceFragment = new ActionChoiceFragment
        fragment.onItemClickedListener = (_, _, _, id) => {
          taskerOption.action = getResources.getStringArray(R.array.tasker_action_value)(id.toInt)
          loadFragment()
          actionSelected = true
        }
        fragment
    }

    getSupportFragmentManager.beginTransaction()
      .replace(R.id.fragment, fragment)
      .commit()
  }

  private def saveResult() {
    if (actionSelected) {
      val fragment: OptionFragment = getSupportFragmentManager.findFragmentById(R.id.fragment).asInstanceOf[OptionFragment]
      if (fragment.saveResult()) {
        setResult(Activity.RESULT_OK, taskerOption.toIntent(this))
        finish()
      }
    } else {
      Toast.makeText(this, R.string.no_action_selected, Toast.LENGTH_SHORT).show()
    }
  }

  private class ActionChoiceFragment extends ListFragment {
    var onItemClickedListener: OnItemClickListener = _

    override def onActivityCreated(savedInstanceState: Bundle) {
      super.onActivityCreated(savedInstanceState)

      val adapter = ArrayAdapter.createFromResource(getActivity, R.array.tasker_action_name, android.R.layout.simple_selectable_list_item)

      setListAdapter(adapter)
    }

    override def onListItemClick(l: ListView, v: View, position: Int, id: Long) {
      if (onItemClickedListener != null) {
        onItemClickedListener.onItemClick(l, v, position, id)
      }
    }
  }

  private class ToggleServiceFragment extends Fragment with OptionFragment {
    var switch: Switch = _

    override def onCreateView(inflater: LayoutInflater, container: ViewGroup, savedInstanceState: Bundle): View = {
      val view: View = inflater.inflate(R.layout.layout_tasker_toggle_service, container, false)

      switch = view.findViewById(R.id.service_switch).asInstanceOf[Switch]
      view
    }

    override def onActivityCreated(savedInstanceState: Bundle) {
      super.onActivityCreated(savedInstanceState)

      if (!taskerOption.isEmpty) {
        switch.setChecked(taskerOption.isStart)
      }
    }

    override def saveResult() = {
      taskerOption.action = TaskerSettings.ACTION_TOGGLE_SERVICE
      taskerOption.isStart = switch.isChecked
      true
    }
  }

  private class ProfileChoiceFragment extends ListFragment with OptionFragment {
    override def onActivityCreated(savedInstanceState: Bundle) {
      super.onActivityCreated(savedInstanceState)

      import scala.collection.JavaConverters._
      val profiles = ShadowsocksApplication.profileManager.getAllProfiles.getOrElse(List.empty)
      val adapter = new ArrayAdapter[Profile](getActivity, 0, android.R.id.text1, profiles.asJava) {
        override def getView(position: Int, convertView: View, parent: ViewGroup): View = {
          val view: TextView = if (convertView == null) {
            val inflater = LayoutInflater.from(parent.getContext)
            inflater.inflate(android.R.layout.simple_list_item_single_choice, parent, false).asInstanceOf[TextView]
          } else {
            convertView.asInstanceOf[TextView]
          }

          view.setText(getItem(position).name)
          view
        }
      }

      setListAdapter(adapter)

      val listView: ListView = getListView
      listView.setChoiceMode(AbsListView.CHOICE_MODE_SINGLE)

      if (!taskerOption.isEmpty) {
        val index = profiles.indexWhere(_.id == taskerOption.profileId)
        if (index > -1) {
          listView.setItemChecked(index, true)
        }
      }
    }

    override def saveResult() = {
      taskerOption.action = TaskerSettings.ACTION_SWITCH_PROFILE
      val listView = getListView
      val item = listView.getItemAtPosition(listView.getCheckedItemPosition)
      if (item == null) {
        Toast.makeText(getActivity, R.string.no_profile_selected, Toast.LENGTH_SHORT).show()
        false
      } else {
        taskerOption.profileId = item.asInstanceOf[Profile].id
        true
      }
    }
  }

  trait OptionFragment extends Fragment {
    def saveResult(): Boolean
  }
}

