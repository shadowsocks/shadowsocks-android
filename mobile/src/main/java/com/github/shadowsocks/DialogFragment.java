package com.github.shadowsocks;

import android.app.Activity;

/**
 * This helper class is used to solve Java-Scala interop problem. Use onAttach(Context) and remove this class
 * when minSdkVersion >= 23.
 *
 * @author Mygod
 */
public class DialogFragment extends android.app.DialogFragment {
    protected void superOnAttach(Activity activity) {
        super.onAttach(activity);
    }
}
