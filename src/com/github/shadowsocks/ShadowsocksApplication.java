package com.github.shadowsocks;

import android.app.Application;
import com.google.analytics.tracking.android.EasyTracker;

public class ShadowsocksApplication extends Application {

    @Override
    public void onCreate() {
        EasyTracker.getInstance().setContext(this);
    }

}
