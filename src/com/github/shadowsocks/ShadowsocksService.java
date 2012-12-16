/* gaeproxy - GAppProxy / WallProxy client App for Android
 * Copyright (C) 2011 <max.c.lv@gmail.com>
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

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.PowerManager;
import android.preference.PreferenceManager;
import android.util.Log;
import com.google.analytics.tracking.android.EasyTracker;

import java.io.DataOutputStream;
import java.lang.ref.WeakReference;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.HashSet;

public class ShadowsocksService extends Service {

    public static final String BASE = "/data/data/com.github.shadowsocks/";
    final static String REDSOCKS_CONF = "base {" +
            " log_debug = off;" +
            " log_info = off;" +
            " log = stderr;" +
            " daemon = on;" +
            " redirector = iptables;" +
            "}" +
            "redsocks {" +
            " local_ip = 127.0.0.1;" +
            " local_port = 8123;" +
            " ip = 127.0.0.1;" +
            " port = %d;" +
            " type = socks5;" +
            "}";
    final static String CMD_IPTABLES_RETURN = " -t nat -A OUTPUT -p tcp -d 0.0.0.0 -j RETURN\n";
    final static String CMD_IPTABLES_REDIRECT_ADD_HTTP = " -t nat -A OUTPUT -p tcp "
            + "--dport 80 -j REDIRECT --to 8123\n";
    final static String CMD_IPTABLES_REDIRECT_ADD_HTTPS = " -t nat -A OUTPUT -p tcp "
            + "--dport 443 -j REDIRECT --to 8124\n";
    final static String CMD_IPTABLES_DNAT_ADD_HTTP = " -t nat -A OUTPUT -p tcp "
            + "--dport 80 -j DNAT --to-destination 127.0.0.1:8123\n";
    final static String CMD_IPTABLES_DNAT_ADD_HTTPS = " -t nat -A OUTPUT -p tcp "
            + "--dport 443 -j DNAT --to-destination 127.0.0.1:8124\n";
    private static final int MSG_CONNECT_START = 0;
    private static final int MSG_CONNECT_FINISH = 1;
    private static final int MSG_CONNECT_SUCCESS = 2;
    private static final int MSG_CONNECT_FAIL = 3;
    private static final int MSG_HOST_CHANGE = 4;
    private static final int MSG_STOP_SELF = 5;
    private static final String TAG = "ShadowsocksService";
    private final static int DNS_PORT = 8153;
    private static final Class<?>[] mStartForegroundSignature = new Class[]{
            int.class, Notification.class};
    private static final Class<?>[] mStopForegroundSignature = new Class[]{boolean.class};
    private static final Class<?>[] mSetForegroundSignature = new Class[]{boolean.class};
    /*
      * This is a hack see
      * http://www.mail-archive.com/android-developers@googlegroups
      * .com/msg18298.html we are not really able to decide if the service was
      * started. So we remember a week reference to it. We set it if we are
      * running and clear it if we are stopped. If anything goes wrong, the
      * reference will hopefully vanish
      */
    private static WeakReference<ShadowsocksService> sRunningInstance = null;
    final Handler handler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            Editor ed = settings.edit();
            switch (msg.what) {
                case MSG_CONNECT_START:
                    ed.putBoolean("isConnecting", true);

                    PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
                    mWakeLock = pm.newWakeLock(PowerManager.SCREEN_DIM_WAKE_LOCK
                            | PowerManager.ON_AFTER_RELEASE, "GAEProxy");

                    mWakeLock.acquire();

                    break;
                case MSG_CONNECT_FINISH:
                    ed.putBoolean("isConnecting", false);

                    if (mWakeLock != null && mWakeLock.isHeld())
                        mWakeLock.release();

                    break;
                case MSG_CONNECT_SUCCESS:
                    ed.putBoolean("isRunning", true);
                    break;
                case MSG_CONNECT_FAIL:
                    ed.putBoolean("isRunning", false);
                    break;
                case MSG_HOST_CHANGE:
                    ed.putString("appHost", appHost);
                    break;
                case MSG_STOP_SELF:
                    stopSelf();
                    break;
            }
            ed.commit();
            super.handleMessage(msg);
        }
    };
    private Notification notification;
    private NotificationManager notificationManager;
    private PendingIntent pendIntent;
    private PowerManager.WakeLock mWakeLock;
    private Process httpProcess = null;
    private DataOutputStream httpOS = null;
    private String appHost;
    private int remotePort;
    private int port;
    private String sitekey;
    private SharedPreferences settings = null;
    private boolean hasRedirectSupport = true;
    private boolean isGlobalProxy = false;
    private boolean isGFWList = false;
    private boolean isBypassApps = false;
    private ProxyedApp apps[];
    private Method mSetForeground;
    private Method mStartForeground;
    private Method mStopForeground;
    private Object[] mSetForegroundArgs = new Object[1];
    private Object[] mStartForegroundArgs = new Object[2];
    private Object[] mStopForegroundArgs = new Object[1];

    public static boolean isServiceStarted() {
        final boolean isServiceStarted;
        if (sRunningInstance == null) {
            isServiceStarted = false;
        } else if (sRunningInstance.get() == null) {
            isServiceStarted = false;
            sRunningInstance = null;
        } else {
            isServiceStarted = true;
        }
        return isServiceStarted;
    }

    public void startShadowsocksDaemon() {
        final String cmd = String.format(BASE
                + "shadowsocks \"%s\" \"%d\" \"%d\" \"%s\"",
                appHost, remotePort, port, sitekey);
        Utils.runRootCommand(cmd);
    }

    public void startDnsDaemon() {
        final String cmd = BASE + "pdnsd -c " + BASE + "pdnsd.conf";
        Utils.runRootCommand(cmd);
    }

    private String getVersionName() {
        String version;
        try {
            PackageInfo pi = getPackageManager().getPackageInfo(
                    getPackageName(), 0);
            version = pi.versionName;
        } catch (PackageManager.NameNotFoundException e) {
            version = "Package name not found";
        }
        return version;
    }

    public void handleCommand(Intent intent) {

        if (intent == null) {
            stopSelf();
            return;
        }

        appHost = settings.getString("proxy", "127.0.0.1");
        sitekey = settings.getString("sitekey", "default");
        try {
            remotePort = Integer.valueOf(settings.getString("remotePort", "1984"));
        } catch (NumberFormatException ex) {
            remotePort = 1984;
        }
        try {
            port = Integer.valueOf(settings.getString("port", "1984"));
        } catch (NumberFormatException ex) {
            port = 1984;
        }

        isGlobalProxy = settings.getBoolean("isGlobalProxy", false);
        isGFWList = settings.getBoolean("isGFWList", false);
        isBypassApps = settings.getBoolean("isBypassApps", false);

        new Thread(new Runnable() {
            @Override
            public void run() {

                handler.sendEmptyMessage(MSG_CONNECT_START);

                Log.d(TAG, "IPTABLES: " + Utils.getIptables());

                // Test for Redirect Support
                hasRedirectSupport = Utils.getHasRedirectSupport();

                if (handleConnection()) {
                    // Connection and forward successful
                    notifyAlert(getString(R.string.forward_success),
                            getString(R.string.service_running));

                    handler.sendEmptyMessageDelayed(MSG_CONNECT_SUCCESS, 500);


                } else {
                    // Connection or forward unsuccessful
                    notifyAlert(getString(R.string.forward_fail),
                            getString(R.string.service_failed));

                    stopSelf();
                    handler.sendEmptyMessageDelayed(MSG_CONNECT_FAIL, 500);
                }

                handler.sendEmptyMessageDelayed(MSG_CONNECT_FINISH, 500);

            }
        }).start();
        markServiceStarted();
    }

    private void startRedsocksDaemon() {
        String conf = String.format(REDSOCKS_CONF, port);
        String cmd = String.format("%sredsocks -p %sredsocks.pid -c %sredsocks.conf",
                BASE, BASE, BASE);
        Utils.runRootCommand("echo \"" + conf + "\" > " + BASE + "redsocks.conf\n"
                + cmd);

    }

    /**
     * Called when the activity is first created.
     */
    public boolean handleConnection() {

        startShadowsocksDaemon();
        startDnsDaemon();
        startRedsocksDaemon();
        setupIptables();

        return true;
    }

    private void initSoundVibrateLights(Notification notification) {
        notification.sound = null;
        notification.defaults |= Notification.DEFAULT_LIGHTS;
    }

    void invokeMethod(Method method, Object[] args) {
        try {
            method.invoke(this, mStartForegroundArgs);
        } catch (InvocationTargetException e) {
            // Should not happen.
            Log.w(TAG, "Unable to invoke method", e);
        } catch (IllegalAccessException e) {
            // Should not happen.
            Log.w(TAG, "Unable to invoke method", e);
        }
    }

    private void markServiceStarted() {
        sRunningInstance = new WeakReference<ShadowsocksService>(this);
    }

    private void markServiceStopped() {
        sRunningInstance = null;
    }

    private void notifyAlert(String title, String info) {
        notification.icon = R.drawable.ic_stat_gaeproxy;
        notification.tickerText = title;
        notification.flags = Notification.FLAG_ONGOING_EVENT;
        initSoundVibrateLights(notification);
        // notification.defaults = Notification.DEFAULT_SOUND;
        notification.setLatestEventInfo(this, getString(R.string.app_name),
                info, pendIntent);
        startForegroundCompat(1, notification);
    }

    private void notifyAlert(String title, String info, int flags) {
        notification.icon = R.drawable.ic_stat_gaeproxy;
        notification.tickerText = title;
        notification.flags = flags;
        initSoundVibrateLights(notification);
        notification.setLatestEventInfo(this, getString(R.string.app_name),
                info, pendIntent);
        notificationManager.notify(0, notification);
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public void onCreate() {
        super.onCreate();

        EasyTracker.getTracker().trackEvent("service", "start",
                getVersionName(), 0L);

        settings = PreferenceManager.getDefaultSharedPreferences(this);
        notificationManager = (NotificationManager) this
                .getSystemService(NOTIFICATION_SERVICE);

        Intent intent = new Intent(this, Shadowsocks.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        pendIntent = PendingIntent.getActivity(this, 0, intent, 0);
        notification = new Notification();

        try {
            mStartForeground = getClass().getMethod("startForeground",
                    mStartForegroundSignature);
            mStopForeground = getClass().getMethod("stopForeground",
                    mStopForegroundSignature);
        } catch (NoSuchMethodException e) {
            // Running on an older platform.
            mStartForeground = mStopForeground = null;
        }

        try {
            mSetForeground = getClass().getMethod("setForeground",
                    mSetForegroundSignature);
        } catch (NoSuchMethodException e) {
            throw new IllegalStateException(
                    "OS doesn't have Service.startForeground OR Service.setForeground!");
        }
    }

    /**
     * Called when the activity is closed.
     */
    @Override
    public void onDestroy() {

        EasyTracker.getTracker().trackEvent("service", "stop",
                getVersionName(), 0L);

        stopForegroundCompat(1);

        notifyAlert(getString(R.string.forward_stop),
                getString(R.string.service_stopped),
                Notification.FLAG_AUTO_CANCEL);

        try {
            if (httpOS != null) {
                httpOS.close();
                httpOS = null;
            }
            if (httpProcess != null) {
                httpProcess.destroy();
                httpProcess = null;
            }
        } catch (Exception e) {
            Log.e(TAG, "HTTP Server close unexpected");
        }

        new Thread() {
            @Override
            public void run() {

                // Make sure the connection is closed, important here
                onDisconnect();

            }
        }.start();

        Editor ed = settings.edit();
        ed.putBoolean("isRunning", false);
        ed.putBoolean("isConnecting", false);
        ed.commit();

        try {
            notificationManager.cancel(0);
        } catch (Exception ignore) {
            // Nothing
        }

        super.onDestroy();

        markServiceStopped();
    }

    private void onDisconnect() {
        Utils.runRootCommand(Utils.getIptables() + " -t nat -F OUTPUT");
        StringBuilder sb = new StringBuilder();
        sb.append("kill -9 `cat /data/data/com.github.shadowsocks/pdnsd.pid`").append("\n");
        sb.append("kill -9 `cat /data/data/com.github.shadowsocks/redsocks.pid`").append("\n");
        sb.append("kill -9 `cat /data/data/com.github.shadowsocks/shadowsocks.pid`").append("\n");
        Utils.runRootCommand(sb.toString());
    }

    // This is the old onStart method that will be called on the pre-2.0
    // platform. On 2.0 or later we override onStartCommand() so this
    // method will not be called.
    @Override
    public void onStart(Intent intent, int startId) {

        handleCommand(intent);

    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        handleCommand(intent);
        // We want this service to continue running until it is explicitly
        // stopped, so return sticky.
        return START_STICKY;
    }

    private boolean setupIptables() {

        StringBuilder init_sb = new StringBuilder();

        StringBuilder http_sb = new StringBuilder();

        StringBuilder https_sb = new StringBuilder();

        init_sb.append(Utils.getIptables()).append(" -t nat -F OUTPUT\n");

        if (hasRedirectSupport) {
            init_sb.append(Utils.getIptables()).append(" -t nat -A OUTPUT -p udp --dport 53 -j REDIRECT --to ").append(DNS_PORT).append("\n");
        } else {
            init_sb.append(Utils.getIptables()).append(" -t nat -A OUTPUT -p udp --dport 53 -j DNAT --to-destination 127.0.0.1:").append(DNS_PORT).append("\n");
        }

        String cmd_bypass = Utils.getIptables() + CMD_IPTABLES_RETURN;

        init_sb.append(cmd_bypass.replace("-d 0.0.0.0", "--dport " + remotePort));

        init_sb.append(cmd_bypass.replace("-d 0.0.0.0", "-m owner --uid-owner "
                + getApplicationInfo().uid));

        if (isGFWList) {
            String[] chn_list = getResources().getStringArray(R.array.chn_list);

            for (String item : chn_list) {
                init_sb.append(cmd_bypass.replace("0.0.0.0", item));
            }
        }
        if (isGlobalProxy || isBypassApps) {
            http_sb.append(hasRedirectSupport ? Utils.getIptables()
                    + CMD_IPTABLES_REDIRECT_ADD_HTTP : Utils.getIptables()
                    + CMD_IPTABLES_DNAT_ADD_HTTP);
            https_sb.append(hasRedirectSupport ? Utils.getIptables()
                    + CMD_IPTABLES_REDIRECT_ADD_HTTPS : Utils.getIptables()
                    + CMD_IPTABLES_DNAT_ADD_HTTPS);
        }
        if (!isGlobalProxy) {
            // for proxy specified apps
            if (apps == null || apps.length <= 0)
                apps = AppManager.getProxyedApps(this);

            HashSet<Integer> uidSet = new HashSet<Integer>();
            for (ProxyedApp app : apps) {
                if (app.isProxyed()) {
                    uidSet.add(app.getUid());
                }
            }
            for (int uid : uidSet) {
                if (!isBypassApps) {
                    http_sb.append((hasRedirectSupport ? Utils.getIptables()
                            + CMD_IPTABLES_REDIRECT_ADD_HTTP : Utils.getIptables()
                            + CMD_IPTABLES_DNAT_ADD_HTTP).replace("-t nat",
                            "-t nat -m owner --uid-owner " + uid));
                    https_sb.append((hasRedirectSupport ? Utils.getIptables()
                            + CMD_IPTABLES_REDIRECT_ADD_HTTPS : Utils.getIptables()
                            + CMD_IPTABLES_DNAT_ADD_HTTPS).replace("-t nat",
                            "-t nat -m owner --uid-owner " + uid));
                } else {
                    init_sb.append(cmd_bypass.replace("-d 0.0.0.0", "-m owner --uid-owner " + uid));
                }
            }
        }

        String init_rules = init_sb.toString();
        Utils.runRootCommand(init_rules, 30 * 1000);

        String redt_rules = http_sb.toString();

        redt_rules += https_sb.toString();

        Utils.runRootCommand(redt_rules);

        return true;

    }

    /**
     * This is a wrapper around the new startForeground method, using the older
     * APIs if it is not available.
     */
    void startForegroundCompat(int id, Notification notification) {
        // If we have the new startForeground API, then use it.
        if (mStartForeground != null) {
            mStartForegroundArgs[0] = id;
            mStartForegroundArgs[1] = notification;
            invokeMethod(mStartForeground, mStartForegroundArgs);
            return;
        }

        // Fall back on the old API.
        mSetForegroundArgs[0] = Boolean.TRUE;
        invokeMethod(mSetForeground, mSetForegroundArgs);
        notificationManager.notify(id, notification);
    }

    /**
     * This is a wrapper around the new stopForeground method, using the older
     * APIs if it is not available.
     */
    void stopForegroundCompat(int id) {
        // If we have the new stopForeground API, then use it.
        if (mStopForeground != null) {
            mStopForegroundArgs[0] = Boolean.TRUE;
            try {
                mStopForeground.invoke(this, mStopForegroundArgs);
            } catch (InvocationTargetException e) {
                // Should not happen.
                Log.w(TAG, "Unable to invoke stopForeground", e);
            } catch (IllegalAccessException e) {
                // Should not happen.
                Log.w(TAG, "Unable to invoke stopForeground", e);
            }
            return;
        }

        // Fall back on the old API. Note to cancel BEFORE changing the
        // foreground state, since we could be killed at that point.
        notificationManager.cancel(id);
        mSetForegroundArgs[0] = Boolean.FALSE;
        invokeMethod(mSetForeground, mSetForegroundArgs);
    }

}
