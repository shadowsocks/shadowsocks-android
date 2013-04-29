/* Shadowsocks - A shadowsocks client for Android
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
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.AssetManager;
import android.graphics.Typeface;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.preference.Preference;
import android.preference.PreferenceManager;
import android.util.Log;
import android.view.KeyEvent;
import android.widget.CompoundButton;
import android.widget.RelativeLayout;
import android.widget.TextView;
import com.actionbarsherlock.view.Menu;
import com.actionbarsherlock.view.MenuItem;
import com.google.analytics.tracking.android.EasyTracker;
import de.keyboardsurfer.android.widget.crouton.Crouton;
import de.keyboardsurfer.android.widget.crouton.Style;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Hashtable;
import net.saik0.android.unifiedpreference.UnifiedPreferenceFragment;
import net.saik0.android.unifiedpreference.UnifiedSherlockPreferenceActivity;
import org.jraf.android.backport.switchwidget.Switch;

public class Shadowsocks extends UnifiedSherlockPreferenceActivity
    implements CompoundButton.OnCheckedChangeListener, OnSharedPreferenceChangeListener {

  public static final String PREFS_NAME = "Shadowsocks";
  public static final String[] PROXY_PREFS = {
      "proxy", "remotePort", "port", "sitekey", "encMethod"
  };
  public static final String[] FEATRUE_PREFS = {
      "isHTTPProxy", "isDNSProxy", "isGFWList", "isGlobalProxy", "isBypassApps", "proxyedApps",
      "isAutoConnect"
  };
  private static final String TAG = "Shadowsocks";
  private static final int MSG_CRASH_RECOVER = 1;
  private static final int MSG_INITIAL_FINISH = 2;
  private static ProgressDialog mProgressDialog = null;
  final Handler handler = new Handler() {
    @Override
    public void handleMessage(Message msg) {
      SharedPreferences settings = PreferenceManager.getDefaultSharedPreferences(Shadowsocks.this);
      Editor ed = settings.edit();
      switch (msg.what) {
        case MSG_CRASH_RECOVER:
          Crouton.makeText(Shadowsocks.this, R.string.crash_alert, Style.ALERT).show();
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
  private Switch switchButton;

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
          if (path.length() > 0) {
            in = assetManager.open(path + "/" + file);
          } else {
            in = assetManager.open(file);
          }
          out = new FileOutputStream("/data/data/com.github.shadowsocks/" + file);
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

    StringBuilder sb = new StringBuilder();
    sb.append(Utils.getIptables()).append(" -t nat -F OUTPUT").append("\n");
    sb.append("kill -9 `cat /data/data/com.github.shadowsocks/pdnsd.pid`").append("\n");
    sb.append("kill -9 `cat /data/data/com.github.shadowsocks/redsocks.pid`").append("\n");
    sb.append("kill -9 `cat /data/data/com.github.shadowsocks/shadowsocks.pid`").append("\n");
    sb.append("kill -9 `cat /data/data/com.github.shadowsocks/polipo.pid`").append("\n");
    sb.append("killall -9 pdnsd").append("\n");
    sb.append("killall -9 redsocks").append("\n");
    sb.append("killall -9 shadowsocks").append("\n");
    sb.append("killall -9 polipo").append("\n");
    Utils.runRootCommand(sb.toString());
  }

  private boolean isTextEmpty(String s, String msg) {
    if (s == null || s.length() <= 0) {
      showAToast(msg);
      return true;
    }
    return false;
  }

  @Override
  public void onCheckedChanged(CompoundButton compoundButton, boolean b) {
    if (compoundButton == switchButton) {
      if (!serviceStart()) {
        switchButton.setChecked(false);
      }
    }
  }

  /** Called when the activity is first created. */
  @Override
  public void onCreate(Bundle savedInstanceState) {
    setHeaderRes(R.xml.shadowsocks_headers);
    super.onCreate(savedInstanceState);

    RelativeLayout switchLayout =
        (RelativeLayout) getLayoutInflater().inflate(R.layout.layout_switch, null);

    getSupportActionBar().setCustomView(switchLayout);
    getSupportActionBar().setDisplayShowTitleEnabled(false);
    getSupportActionBar().setDisplayShowCustomEnabled(true);
    getSupportActionBar().setDisplayShowHomeEnabled(false);

    switchButton = (Switch) switchLayout.findViewById(R.id.switchButton);

    TextView title = (TextView) switchLayout.findViewById(R.id.title);
    Typeface tf = Typefaces.get(this, "fonts/Iceland.ttf");
    if (tf != null) title.setTypeface(tf);
    title.setText(R.string.app_name);

    final SharedPreferences settings =
        PreferenceManager.getDefaultSharedPreferences(Shadowsocks.this);

    boolean init =
        !settings.getBoolean("isRunning", false) && !settings.getBoolean("isConnecting", false);

    if (init) {
      if (mProgressDialog == null) {
        mProgressDialog =
            ProgressDialog.show(this, "", getString(R.string.initializing), true, true);
      }

      new Thread() {
        @Override
        public void run() {

          Utils.isRoot();

          String versionName;
          try {
            versionName = getPackageManager().getPackageInfo(getPackageName(), 0).versionName;
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
  }

  @Override
  public boolean onCreateOptionsMenu(Menu menu) {
    menu.add(0, 0, 0, R.string.recovery)
        .setIcon(android.R.drawable.ic_menu_revert)
        .setShowAsAction(MenuItem.SHOW_AS_ACTION_WITH_TEXT);
    menu.add(0, 1, 1, R.string.about)
        .setIcon(android.R.drawable.ic_menu_info_details)
        .setShowAsAction(MenuItem.SHOW_AS_ACTION_WITH_TEXT);
    return true;
  }

  /** Called when the activity is closed. */
  @Override
  public void onDestroy() {
    SharedPreferences settings = getSharedPreferences(PREFS_NAME, 0);

    SharedPreferences.Editor editor = settings.edit();
    editor.putBoolean("isConnected", ShadowsocksService.isServiceStarted());
    editor.commit();

    super.onDestroy();
  }

  @Override
  public boolean onKeyDown(int keyCode, KeyEvent event) {
    if (keyCode == KeyEvent.KEYCODE_BACK && event.getRepeatCount() == 0) {
      try {
        finish();
      } catch (Exception ignore) {
        // Nothing
      }
      return true;
    }
    return super.onKeyDown(keyCode, event);
  }

  @Override
  public boolean onOptionsItemSelected(MenuItem item) {
    switch (item.getItemId()) {
      case 0:
        recovery();
        break;
      case 1:
        String versionName = "";
        try {
          versionName = getPackageManager().getPackageInfo(getPackageName(), 0).versionName;
        } catch (NameNotFoundException e) {
          versionName = "";
        }
        showAToast(getString(R.string.about) + " (" + versionName + ")\n\n" + getString(
            R.string.copy_rights));
        break;
    }

    return super.onOptionsItemSelected(item);
  }

  @Override
  protected void onPause() {
    super.onPause();
    PreferenceManager.getDefaultSharedPreferences(this)
        .unregisterOnSharedPreferenceChangeListener(this);
  }

  @Override
  protected void onResume() {
    super.onResume();
    SharedPreferences settings = PreferenceManager.getDefaultSharedPreferences(this);

    Editor edit = settings.edit();

    if (ShadowsocksService.isServiceStarted()) {
      edit.putBoolean("isRunning", true);
      switchButton.setChecked(true);
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
      switchButton.setChecked(false);
    }

    edit.commit();

    setPreferenceEnabled();

    switchButton.setOnCheckedChangeListener(this);
    PreferenceManager.getDefaultSharedPreferences(this)
        .registerOnSharedPreferenceChangeListener(this);
  }

  private void setPreferenceEnabled() {
    SharedPreferences settings = PreferenceManager.getDefaultSharedPreferences(Shadowsocks.this);

    boolean enabled =
        !settings.getBoolean("isRunning", false) && !settings.getBoolean("isConnecting", false);

    for (String name : PROXY_PREFS) {
      Preference pref = findPreference(name);
      if (pref != null) {
        pref.setEnabled(enabled);
      }
    }
    for (String name : FEATRUE_PREFS) {
      Preference pref = findPreference(name);
      if (pref != null) {
        if (name.equals("isBypassApps") || name.equals("proxyedApps")) {
          boolean isGlobalProxy = settings.getBoolean("isGlobalProxy", false);
          pref.setEnabled(enabled && !isGlobalProxy);
        } else {
          pref.setEnabled(enabled);
        }
      }
    }
  }

  @Override
  public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key) {
    // Let's do something a preference value changes
    SharedPreferences settings = PreferenceManager.getDefaultSharedPreferences(this);

    if (key.equals("isRunning")) {
      if (settings.getBoolean("isRunning", false)) {
        switchButton.setChecked(true);
      } else {
        switchButton.setChecked(false);
      }
    }

    if (key.equals("isConnecting")) {
      if (settings.getBoolean("isConnecting", false)) {
        Log.d(TAG, "Connecting start");
        if (mProgressDialog == null) {
          mProgressDialog =
              ProgressDialog.show(this, "", getString(R.string.connecting), true, true);
        }
      } else {
        Log.d(TAG, "Connecting finish");
        if (mProgressDialog != null) {
          mProgressDialog.dismiss();
          mProgressDialog = null;
        }
      }
    }

    if (key.equals("isRunning") || key.equals("isGlobalProxy")) {
      setPreferenceEnabled();
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

    if (mProgressDialog != null) {
      mProgressDialog.dismiss();
      mProgressDialog = null;
    }
  }

  public void reset() {

    crash_recovery();

    copyAssets("");
    copyAssets(Utils.getABI());

    Utils.runCommand("chmod 755 /data/data/com.github.shadowsocks/iptables\n"
        + "chmod 755 /data/data/com.github.shadowsocks/redsocks\n"
        + "chmod 755 /data/data/com.github.shadowsocks/pdnsd\n"
        + "chmod 755 /data/data/com.github.shadowsocks/shadowsocks\n"
        + "chmod 755 /data/data/com.github.shadowsocks/polipo\n");
  }

  private void recovery() {

    if (mProgressDialog == null) {
      mProgressDialog = ProgressDialog.show(this, "", getString(R.string.recovering), true, true);
    }

    final Handler h = new Handler() {
      @Override
      public void handleMessage(Message msg) {
        if (mProgressDialog != null) {
          mProgressDialog.dismiss();
          mProgressDialog = null;
        }
      }
    };

    stopService(new Intent(this, ShadowsocksService.class));

    new Thread() {
      @Override
      public void run() {

        reset();

        h.sendEmptyMessage(0);
      }
    }.start();
  }

  /** Called when connect button is clicked. */
  public boolean serviceStart() {

    if (ShadowsocksService.isServiceStarted()) {
      stopService(new Intent(this, ShadowsocksService.class));
      return false;
    }

    SharedPreferences settings = PreferenceManager.getDefaultSharedPreferences(this);

    final String proxy = settings.getString("proxy", "");
    if (isTextEmpty(proxy, getString(R.string.proxy_empty))) return false;

    String portText = settings.getString("port", "");
    if (isTextEmpty(portText, getString(R.string.port_empty))) return false;
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
        .setNegativeButton(getString(R.string.ok_iknow), new DialogInterface.OnClickListener() {
          @Override
          public void onClick(DialogInterface dialog, int id) {
            dialog.cancel();
          }
        });
    AlertDialog alert = builder.create();
    alert.show();
  }

  public static class ProxyFragment extends UnifiedPreferenceFragment
      implements OnSharedPreferenceChangeListener {

    private void setPreferenceEnabled() {
      SharedPreferences settings = PreferenceManager.getDefaultSharedPreferences(getActivity());

      boolean enabled =
          !settings.getBoolean("isRunning", false) && !settings.getBoolean("isConnecting", false);

      for (String name : PROXY_PREFS) {
        Preference pref = findPreference(name);
        if (pref != null) {
          pref.setEnabled(enabled);
        }
      }
    }

    @Override
    public void onResume() {
      super.onResume();
      setPreferenceEnabled();

      // Set up a listener whenever a key changes
      getPreferenceScreen().getSharedPreferences().registerOnSharedPreferenceChangeListener(this);
    }

    @Override
    public void onPause() {
      super.onPause();
      // Unregister the listener whenever a key changes
      getPreferenceScreen().getSharedPreferences().unregisterOnSharedPreferenceChangeListener(this);
    }

    @Override
    public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key) {
      if (key.equals("isRunning") || key.equals("isGlobalProxy")) {
        setPreferenceEnabled();
      }
    }
  }

  public static class FeatureFragment extends UnifiedPreferenceFragment
      implements OnSharedPreferenceChangeListener {

    private void setPreferenceEnabled() {
      SharedPreferences settings = PreferenceManager.getDefaultSharedPreferences(getActivity());

      boolean enabled =
          !settings.getBoolean("isRunning", false) && !settings.getBoolean("isConnecting", false);

      for (String name : FEATRUE_PREFS) {
        Preference pref = findPreference(name);
        if (pref != null) {
          if (name.equals("isBypassApps") || name.equals("proxyedApps")) {
            boolean isGlobalProxy = settings.getBoolean("isGlobalProxy", false);
            pref.setEnabled(enabled && !isGlobalProxy);
          } else {
            pref.setEnabled(enabled);
          }
        }
      }
    }

    @Override
    public void onResume() {
      super.onResume();
      setPreferenceEnabled();

      // Set up a listener whenever a key changes
      getPreferenceScreen().getSharedPreferences().registerOnSharedPreferenceChangeListener(this);
    }

    @Override
    public void onPause() {
      super.onPause();
      // Unregister the listener whenever a key changes
      getPreferenceScreen().getSharedPreferences().unregisterOnSharedPreferenceChangeListener(this);
    }

    @Override
    public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key) {
      if (key.equals("isRunning") || key.equals("isGlobalProxy")) {
        setPreferenceEnabled();
      }
    }
  }

  public static class Typefaces {
    private static final String TAG = "Typefaces";

    private static final Hashtable<String, Typeface> cache = new Hashtable<String, Typeface>();

    public static Typeface get(Context c, String assetPath) {
      synchronized (cache) {
        if (!cache.containsKey(assetPath)) {
          try {
            Typeface t = Typeface.createFromAsset(c.getAssets(), assetPath);
            cache.put(assetPath, t);
          } catch (Exception e) {
            Log.e(TAG, "Could not get typeface '" + assetPath + "' because " + e.getMessage());
            return null;
          }
        }
        return cache.get(assetPath);
      }
    }
  }
}
