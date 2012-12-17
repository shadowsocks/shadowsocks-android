package com.github.shadowsocks;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.preference.PreferenceManager;

public class ShadowsocksReceiver extends BroadcastReceiver {

    private static final String TAG = "Shadowsocks";

    @Override
    public void onReceive(Context context, Intent intent) {

        SharedPreferences settings = PreferenceManager.getDefaultSharedPreferences(context);
        String versionName;
        try {
            versionName = context.getPackageManager().getPackageInfo(context.getPackageName(), 0).versionName;
        } catch (PackageManager.NameNotFoundException e) {
            versionName = "NONE";
        }

        final boolean isAutoConnect = settings.getBoolean("isAutoConnect", false);
        final boolean isInstalled = settings.getBoolean(versionName, false);

        if (isAutoConnect && isInstalled) {
            final String portText = settings.getString("port", "");
            if (portText == null || portText.length() <= 0) {
                return;
            }
            try {
                int port = Integer.valueOf(portText);
                if (port <= 1024) {
                    return;
                }
            } catch (Exception e) {
                return;
            }

            Intent it = new Intent(context, ShadowsocksService.class);
            context.startService(it);
        }
    }

}
