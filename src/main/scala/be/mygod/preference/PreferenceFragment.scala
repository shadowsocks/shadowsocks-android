package be.mygod.preference

import android.app.DialogFragment
import android.os.Bundle
import android.support.v14.preference.{PreferenceFragment => Base}
import android.support.v7.preference.{Preference, PreferenceScreen}
import android.view.{LayoutInflater, ViewGroup}

abstract class PreferenceFragment extends Base {
  override def onCreateView(inflater: LayoutInflater, container: ViewGroup, savedInstanceState: Bundle) =
    super.onCreateView(inflater, container, savedInstanceState)

  protected final def displayPreferenceDialog(key: String, fragment: DialogFragment) {
    val bundle = new Bundle(1)
    bundle.putString("key", key)
    fragment.setArguments(bundle)
    fragment.setTargetFragment(this, 0)
    getFragmentManager.beginTransaction()
      .add(fragment, "android.support.v14.preference.PreferenceFragment.DIALOG")
      .commitAllowingStateLoss()
  }

  override def onDisplayPreferenceDialog(preference: Preference) = preference match {
    case dpp: DialogPreferencePlus => displayPreferenceDialog(preference.getKey, dpp.createDialog())
    case _ => super.onDisplayPreferenceDialog(preference)
  }

  override protected def onCreateAdapter(screen: PreferenceScreen) = new PreferenceGroupAdapter(screen)

  override def onResume {
    super.onResume
    getListView.scrollBy(0, 0)
  }
}
