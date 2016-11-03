package be.mygod.preference

import android.app.DialogFragment
import android.support.v7.preference.DialogPreference

trait DialogPreferencePlus extends DialogPreference {
  def createDialog(): DialogFragment
}
