/* Shadowsocks - GoAgent / WallProxy client App for Android
 * Copyright (C) 2012 <max.c.lv@gmail.com>
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

package com.github.shadowsocks;

import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.preference.*;
import android.util.Log;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.Toast;
import com.google.analytics.tracking.android.EasyTracker;

import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class Shadowsocks extends PreferenceActivity implements
        OnSharedPreferenceChangeListener {

    public static final String PREFS_NAME = "Shadowsocks";
    private static final String TAG = "Shadowsocks";
    private static final int MSG_CRASH_RECOVER = 1;
    private static final int MSG_INITIAL_FINISH = 2;
    private static ProgressDialog mProgressDialog = null;
    final Handler handler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            SharedPreferences settings = PreferenceManager
                    .getDefaultSharedPreferences(Shadowsocks.this);
            Editor ed = settings.edit();
            switch (msg.what) {
                case MSG_CRASH_RECOVER:
                    Toast.makeText(Shadowsocks.this, R.string.crash_alert,
                            Toast.LENGTH_LONG).show();
                    ed.putBoolean("isRunning", false);
                    break;
                case MSG_INITIAL_FINISH:
                    if (mProgressDialog != null) {
                        mProgressDialog.dismiss();
                        mProgressDialog = null;
                    }
                    break;
            }
            ed.commit();
            super.handleMessage(msg);
        }
    };
    private EditTextPreference proxyText;
    private EditTextPreference portText;
    private EditTextPreference remotePortText;
    private EditTextPreference sitekeyText;
    private Preference proxyedApps;
    private CheckBoxPreference isBypassAppsCheck;
    private CheckBoxPreference isRunningCheck;

    private void copyAssets(String path) {

        AssetManager assetManager = getAssets();
        String[] files = null;
        try {
            files = assetManager.list(path);
        } catch (IOException e) {
            Log.e(TAG, e.getMessage());
        }
        if (files != null) {
            for (String file : files) {
                InputStream in = null;
                OutputStream out = null;
                try {
                    in = assetManager.open(file);
                    out = new FileOutputStream("/data/data/com.github.shadowsocks/"
                            + file);
                    copyFile(in, out);
                    in.close();
                    in = null;
                    out.flush();
                    out.close();
                    out = null;
                } catch (Exception e) {
                    Log.e(TAG, e.getMessage());
                }
            }
        }
    }

    private void copyFile(InputStream in, OutputStream out) throws IOException {
        byte[] buffer = new byte[1024];
        int read;
        while ((read = in.read(buffer)) != -1) {
            out.write(buffer, 0, read);
        }
    }

    private void crash_recovery() {

        Utils.runRootCommand(Utils.getIptables() + " -t nat -F OUTPUT");

        Utils.runCommand(ShadowsocksService.BASE + "proxy.sh stop");

    }

    private boolean isTextEmpty(String s, String msg) {
        if (s == null || s.length() <= 0) {
            showAToast(msg);
            return true;
        }
        return false;
    }

    /**
     * Called when the activity is first created.
     */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        addPreferencesFromResource(R.xml.shadowsocks_preference);

        proxyText = (EditTextPreference) findPreference("proxy");
        portText = (EditTextPreference) findPreference("port");
        remotePortText = (EditTextPreference) findPreference("remotePort");
        sitekeyText = (EditTextPreference) findPreference("sitekey");
        proxyedApps = findPreference("proxyedApps");

        isBypassAppsCheck = (CheckBoxPreference) findPreference("isBypassApps");
        isRunningCheck = (CheckBoxPreference) findPreference("isRunning");

        if (mProgressDialog == null)
            mProgressDialog = ProgressDialog.show(this, "",
                    getString(R.string.initializing), true, true);

        final SharedPreferences settings = PreferenceManager
                .getDefaultSharedPreferences(this);

        new Thread() {
            @Override
            public void run() {

                Utils.isRoot();

                String versionName;
                try {
                    versionName = getPackageManager().getPackageInfo(
                            getPackageName(), 0).versionName;
                } catch (NameNotFoundException e) {
                    versionName = "NONE";
                }

                if (!settings.getBoolean(versionName, false)) {

                    Editor edit = settings.edit();
                    edit.putBoolean(versionName, true);
                    edit.commit();

                    reset();

                }

                handler.sendEmptyMessage(MSG_INITIAL_FINISH);
            }
        }.start();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        menu.add(Menu.NONE, Menu.FIRST + 1, 1, getString(R.string.recovery))
                .setIcon(android.R.drawable.ic_menu_delete);
        menu.add(Menu.NONE, Menu.FIRST + 2, 2, getString(R.string.about))
                .setIcon(android.R.drawable.ic_menu_info_details);
        return true;

    }

    /**
     * Called when the activity is closed.
     */
    @Override
    public void onDestroy() {
        SharedPreferences settings = getSharedPreferences(PREFS_NAME, 0);

        SharedPreferences.Editor editor = settings.edit();
        editor.putBoolean("isConnected", ShadowsocksService.isServiceStarted());
        editor.commit();

        if (mProgressDialog != null) {
            mProgressDialog.dismiss();
            mProgressDialog = null;
        }

        super.onDestroy();
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_BACK && event.getRepeatCount() == 0) { // 按下的如果是BACK，同时没有重复
            try {
                finish();
            } catch (Exception ignore) {
                // Nothing
            }
            return true;
        }
        return super.onKeyDown(keyCode, event);
    }

    // 菜单项被选择事件
    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case Menu.FIRST + 1:
                recovery();
                break;
            case Menu.FIRST + 2:
                String versionName = "";
                try {
                    versionName = getPackageManager().getPackageInfo(
                            getPackageName(), 0).versionName;
                } catch (NameNotFoundException e) {
                    versionName = "";
                }
                showAToast(getString(R.string.about) + " (" + versionName + ")\n\n"
                        + getString(R.string.copy_rights));
                break;
        }

        return true;
    }

    @Override
    protected void onPause() {
        super.onPause();

        // Unregister the listener whenever a key changes
        getPreferenceScreen().getSharedPreferences()
                .unregisterOnSharedPreferenceChangeListener(this);
    }

    @Override
    public boolean onPreferenceTreeClick(PreferenceScreen preferenceScreen,
                                         Preference preference) {
        SharedPreferences settings = PreferenceManager
                .getDefaultSharedPreferences(this);

        if (preference.getKey() != null
                && preference.getKey().equals("proxyedApps")) {
            Intent intent = new Intent(this, AppManager.class);
            startActivity(intent);
        } else if (preference.getKey() != null
                && preference.getKey().equals("isRunning")) {
            if (!serviceStart()) {
                Editor edit = settings.edit();
                edit.putBoolean("isRunning", false);
                edit.commit();
            }
        }
        return super.onPreferenceTreeClick(preferenceScreen, preference);
    }

    @Override
    protected void onResume() {
        super.onResume();
        SharedPreferences settings = PreferenceManager
                .getDefaultSharedPreferences(this);

        if (settings.getBoolean("isGlobalProxy", false)) {
            proxyedApps.setEnabled(false);
            isBypassAppsCheck.setEnabled(false);
        } else {
            proxyedApps.setEnabled(true);
            isBypassAppsCheck.setEnabled(true);
        }

        Editor edit = settings.edit();

        if (ShadowsocksService.isServiceStarted()) {
            edit.putBoolean("isRunning", true);
        } else {
            if (settings.getBoolean("isRunning", false)) {
                new Thread() {
                    @Override
                    public void run() {
                        crash_recovery();
                        handler.sendEmptyMessage(MSG_CRASH_RECOVER);
                    }
                }.start();
            }
            edit.putBoolean("isRunning", false);
        }

        edit.commit();

        // Setup the initial values

        if (!settings.getString("sitekey", "").equals(""))
            sitekeyText.setSummary(settings.getString("sitekey", ""));

        if (!settings.getString("port", "").equals(""))
            portText.setSummary(settings.getString("port",
                    getString(R.string.port_summary)));

        if (!settings.getString("remotePort", "").equals(""))
            remotePortText.setSummary(settings.getString("remotePort",
                    getString(R.string.remote_port_summary)));

        if (!settings.getString("proxy", "").equals(""))
            proxyText.setSummary(settings.getString("proxy",
                    getString(R.string.proxy_summary)));

        // Set up a listener whenever a key changes
        getPreferenceScreen().getSharedPreferences()
                .registerOnSharedPreferenceChangeListener(this);
    }

    @Override
    public void onSharedPreferenceChanged(SharedPreferences sharedPreferences,
                                          String key) {
        // Let's do something a preference value changes
        SharedPreferences settings = PreferenceManager
                .getDefaultSharedPreferences(this);

        if (key.equals("isRunning")) {
            if (settings.getBoolean("isRunning", false)) {
                isRunningCheck.setChecked(true);
            } else {
                isRunningCheck.setChecked(false);
            }
        }

        if (key.equals("isConnecting")) {
            if (settings.getBoolean("isConnecting", false)) {
                Log.d(TAG, "Connecting start");
                if (mProgressDialog == null)
                    mProgressDialog = ProgressDialog.show(this, "",
                            getString(R.string.connecting), true, true);
            } else {
                Log.d(TAG, "Connecting finish");
                if (mProgressDialog != null) {
                    mProgressDialog.dismiss();
                    mProgressDialog = null;
                }
            }
        }

        if (key.equals("isGlobalProxy")) {
            if (settings.getBoolean("isGlobalProxy", false)) {
                proxyedApps.setEnabled(false);
                isBypassAppsCheck.setEnabled(false);
            } else {
                proxyedApps.setEnabled(true);
                isBypassAppsCheck.setEnabled(true);
            }
        }


        if (key.equals("remotePort"))
            if (settings.getString("remotePort", "").equals(""))
                remotePortText.setSummary(getString(R.string.remote_port_summary));
            else
                remotePortText.setSummary(settings.getString("remotePort", ""));
        else if (key.equals("port"))
            if (settings.getString("port", "").equals(""))
                portText.setSummary(getString(R.string.port_summary));
            else
                portText.setSummary(settings.getString("port", ""));
        else if (key.equals("sitekey"))
            if (settings.getString("sitekey", "").equals(""))
                sitekeyText.setSummary(getString(R.string.sitekey_summary));
            else
                sitekeyText.setSummary(settings.getString("sitekey", ""));
        else if (key.equals("proxy"))
            if (settings.getString("proxy", "").equals("")) {
                proxyText.setSummary(getString(R.string.proxy_summary));
            } else {
                proxyText.setSummary(settings.getString("proxy", ""));
            }
    }

    @Override
    public void onStart() {
        super.onStart();
        EasyTracker.getInstance().activityStart(this);
    }

    @Override
    public void onStop() {
        super.onStop();
        EasyTracker.getInstance().activityStop(this);
    }

    public void reset() {
        StringBuilder sb = new StringBuilder();
        sb.append(Utils.getIptables()).append(" -t nat -F OUTPUT").append("\n");
        sb.append("kill -9 `cat /data/data/com.github.shadowsocks/pdnsd.pid`").append("\n");
        sb.append("kill -9 `cat /data/data/com.github.shadowsocks/redsocks.pid`").append("\n");
        sb.append("kill -9 `cat /data/data/com.github.shadowsocks/shadowsocks.pid`").append("\n");
        sb.append("killall -9 pdnsd").append("\n");
        sb.append("killall -9 redsocks").append("\n");
        sb.append("killall -9 shadowsocks").append("\n");
        Utils.runRootCommand(sb.toString());

        copyAssets("");

        Utils.runCommand("chmod 755 /data/data/com.github.shadowsocks/iptables\n"
                + "chmod 755 /data/data/com.github.shadowsocks/redsocks\n"
                + "chmod 755 /data/data/com.github.shadowsocks/pdnsd\n"
                + "chmod 755 /data/data/com.github.shadowsocks/shadowsocks\n"
        );
    }

    private void recovery() {

        if (mProgressDialog == null)
            mProgressDialog = ProgressDialog.show(this, "", getString(R.string.recovering),
                    true, true);

        final Handler h = new Handler() {
            @Override
            public void handleMessage(Message msg) {
                if (mProgressDialog != null) {
                    mProgressDialog.dismiss();
                    mProgressDialog = null;
                }
            }
        };

        try {
            stopService(new Intent(this, ShadowsocksService.class));
        } catch (Exception e) {
            // Nothing
        }

        new Thread() {
            @Override
            public void run() {

                reset();

                h.sendEmptyMessage(0);
            }
        }.start();

    }

    /**
     * Called when connect button is clicked.
     */
    public boolean serviceStart() {

        if (ShadowsocksService.isServiceStarted()) {
            stopService(new Intent(this, ShadowsocksService.class));
            return false;
        }

        SharedPreferences settings = PreferenceManager
                .getDefaultSharedPreferences(this);

        final String proxy = settings.getString("proxy", "");
        if (isTextEmpty(proxy, getString(R.string.proxy_empty)))
            return false;

        String portText = settings.getString("port", "");
        if (isTextEmpty(portText, getString(R.string.port_empty)))
            return false;
        try {
            int port = Integer.valueOf(portText);
            if (port <= 1024) {
                this.showAToast(getString(R.string.port_alert));
                return false;
            }
        } catch (Exception e) {
            this.showAToast(getString(R.string.port_alert));
            return false;
        }

        Intent it = new Intent(this, ShadowsocksService.class);
        startService(it);

        return true;
    }

    private void showAToast(String msg) {
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setMessage(msg)
                .setCancelable(false)
                .setNegativeButton(getString(R.string.ok_iknow),
                        new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int id) {
                                dialog.cancel();
                            }
                        });
        AlertDialog alert = builder.create();
        alert.show();
    }

}
