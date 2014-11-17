/*
 * Copyright (C) 2012-2014 Jorrit "Chainfire" Jongma
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package eu.chainfire.libsuperuser;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

/**
 * <p>
 * Base receiver to extend to catch notifications when overlays should be
 * hidden.
 * </p>
 * <p>
 * Tapjacking protection in SuperSU prevents some dialogs from receiving user
 * input when overlays are present. For security reasons this notification is
 * only sent to apps that have previously been granted root access, so even if
 * your app does not <em>require</em> root, you still need to <em>request</em>
 * it, and the user must grant it.
 * </p>
 * <p>
 * Note that the word overlay as used here should be interpreted as "any view or
 * window possibly obscuring SuperSU dialogs".
 * </p>
 */
public abstract class HideOverlaysReceiver extends BroadcastReceiver {
    public static final String ACTION_HIDE_OVERLAYS = "eu.chainfire.supersu.action.HIDE_OVERLAYS";
    public static final String CATEGORY_HIDE_OVERLAYS = Intent.CATEGORY_INFO;
    public static final String EXTRA_HIDE_OVERLAYS = "eu.chainfire.supersu.extra.HIDE";

    @Override
    public final void onReceive(Context context, Intent intent) {
        if (intent.hasExtra(EXTRA_HIDE_OVERLAYS)) {
            onHideOverlays(intent.getBooleanExtra(EXTRA_HIDE_OVERLAYS, false));
        }
    }

    /**
     * Called when overlays <em>should</em> be hidden or <em>may</em> be shown
     * again.
     * 
     * @param hide Should overlays be hidden?
     */
    public abstract void onHideOverlays(boolean hide);
}
