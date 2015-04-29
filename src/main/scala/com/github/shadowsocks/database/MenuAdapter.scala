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

package com.github.shadowsocks.database

import android.content.Context
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.BaseAdapter
import android.widget.TextView
import android.graphics.drawable.Drawable
import com.github.shadowsocks.{R}
import android.view.View.{OnLongClickListener, OnClickListener}

abstract class Item {
  def id: Int
  def title: String
  def click: Int => Unit
  def longClick: Int => Boolean
}

case class IconItem(id: Int, title: String, iconRes: Int,
  click: Int => Unit, longClick: Int => Boolean = _ => false)
  extends Item

case class DrawableItem(id: Int, title: String, iconRes: Drawable,
  click: Int => Unit, longClick: Int => Boolean = _ => false)
  extends Item

case class Category(title: String)

object MenuAdapter {

  trait MenuListener {
    def onActiveViewChanged(v: View, pos: Int)
  }

  val UNDEFINED = -1
  val CATEGORY = 0
  val ITEM = 1
}

class MenuAdapter(context: Context, var items: List[Any]) extends BaseAdapter {

  def setListener(listener: MenuAdapter.MenuListener) {
    this.listener = listener
  }

  def setActiveId(activeId: Int) {
    this.activeId = activeId
  }

  @Override def getCount: Int = {
    items.size
  }

  @Override def getItem(position: Int): Object = {
    items(position).asInstanceOf[Object]
  }

  @Override def getItemId(position: Int): Long = {
    position
  }

  @Override override def getItemViewType(position: Int): Int = {
    getItem(position) match {
      case Category => MenuAdapter.CATEGORY
      case IconItem => MenuAdapter.ITEM
      case DrawableItem => MenuAdapter.ITEM
      case _ => MenuAdapter.UNDEFINED
    }
  }

  @Override override def getViewTypeCount: Int = {
    3
  }

  @Override override def isEnabled(position: Int): Boolean = {
    getItemViewType(position) != MenuAdapter.UNDEFINED
  }

  @Override override def areAllItemsEnabled: Boolean = {
    false
  }

  @Override def getView(position: Int, convertView: View, parent: ViewGroup): View = {
    var v: View = convertView
    val item = getItem(position)
    item match {
      case value: Category =>
        if (v == null) {
          v = LayoutInflater.from(context).inflate(R.layout.menu_row_category, parent, false)
        }
        v.asInstanceOf[TextView].setText(value.title)
      case value: Item =>
        if (v == null) {
          v = LayoutInflater.from(context).inflate(R.layout.menu_row_item, parent, false)
        }
        val tv: TextView = v.asInstanceOf[TextView]
        tv.setText(value.title)

        value match {
          case iconItem: IconItem =>
            if (iconItem.iconRes != -1) {
              tv.setCompoundDrawablesWithIntrinsicBounds(iconItem.iconRes, 0, 0, 0)
            }
          case drawableItem: DrawableItem =>
            if (drawableItem.iconRes != null) {
              tv.setCompoundDrawablesWithIntrinsicBounds(drawableItem.iconRes, null, null, null)
            }
        }

        tv.setOnClickListener(new OnClickListener {
          def onClick(view: View) {
            val item = view.getTag(R.id.mdItem).asInstanceOf[Item]
            item.click(item.id)
          }
        })
        tv.setOnLongClickListener(new OnLongClickListener {
          def onLongClick(view: View): Boolean = {
            val item = view.getTag(R.id.mdItem).asInstanceOf[Item]
            item.longClick(item.id)
          }
        })

        v.setTag(R.id.mdItem, value)

        if (value.id == activeId) {
          if (listener != null) listener.onActiveViewChanged(v, position)
        }

      case _ =>
    }
    v.setTag(R.id.mdActiveViewPosition, position)


    v
  }

  def updateList(list: List[Any], activeId: Int) {
    items = list
    this.activeId = activeId
    notifyDataSetChanged()
  }

  private var listener: MenuAdapter.MenuListener = null
  private var activeId: Int = -1
}


