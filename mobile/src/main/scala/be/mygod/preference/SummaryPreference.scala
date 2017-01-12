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

package be.mygod.preference

import android.support.v7.preference.Preference

/**
 * Make your preference support %s in summary. Override getSummaryValue to customize what to put in.
 * Based on:
 * https://github.com/android/platform_frameworks_base/blob/master/core/java/android/preference/ListPreference.java
 *
 * @author Mygod
 */
trait SummaryPreference extends Preference {
  protected def getSummaryValue: AnyRef

  /**
   * Returns the summary of this SummaryPreference. If the summary has a String formatting marker in it
   * (i.e. "%s" or "%1$s"), then the current entry value will be substituted in its place.
   *
   * @return the summary with appropriate string substitution
   */
  override def getSummary: String = {
    val summary = super.getSummary
    if (summary == null) null else String.format(summary.toString, getSummaryValue)
  }
}
