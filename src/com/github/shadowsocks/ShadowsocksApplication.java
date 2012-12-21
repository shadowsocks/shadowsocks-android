package com.github.shadowsocks;

import android.app.Application;
import com.google.analytics.tracking.android.EasyTracker;

public class ShadowsocksApplication extends Application {

    private static String sTmpDir;

    public static String getTmpDir() {
        return sTmpDir;
    }

    @Override
    public void onCreate() {
        EasyTracker.getInstance().setContext(this);
        sTmpDir = getCacheDir().getAbsolutePath();
    }

}
