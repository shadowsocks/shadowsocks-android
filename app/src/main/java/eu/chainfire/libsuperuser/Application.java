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

import android.content.Context;
import android.os.Handler;
import android.widget.Toast;

/**
 * Base application class to extend from, solving some issues with
 * toasts and AsyncTasks you are likely to run into
 */
public class Application extends android.app.Application {
    /**
     * Shows a toast message
     * 
     * @param context Any context belonging to this application
     * @param message The message to show
     */
    public static void toast(Context context, String message) {
        // this is a static method so it is easier to call,
        // as the context checking and casting is done for you

        if (context == null) return;

        if (!(context instanceof Application)) {
            context = context.getApplicationContext();
        }

        if (context instanceof Application) {
            final Context c = context;
            final String m = message;

            ((Application)context).runInApplicationThread(new Runnable() {
                @Override
                public void run() {
                    Toast.makeText(c, m, Toast.LENGTH_LONG).show();
                }
            });
        }
    }

    private static Handler mApplicationHandler = new Handler();

    /**
     * Run a runnable in the main application thread
     * 
     * @param r Runnable to run
     */
    public void runInApplicationThread(Runnable r) {
        mApplicationHandler.post(r);
    }

    @Override
    public void onCreate() {
        super.onCreate();

        try {
            // workaround bug in AsyncTask, can show up (for example) when you toast from a service
            // this makes sure AsyncTask's internal handler is created from the right (main) thread
            Class.forName("android.os.AsyncTask");
        } catch (ClassNotFoundException e) {
        }
    }
}
