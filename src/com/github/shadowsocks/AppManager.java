/* Copyright (c) 2009, Nathan Freitas, Orbot / The Guardian Project - http://openideals.com/guardian */
/* See LICENSE for licensing information */

package com.github.shadowsocks;

import android.app.Activity;
import android.app.ProgressDialog;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.graphics.PixelFormat;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.preference.PreferenceManager;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.WindowManager;
import android.widget.*;
import android.widget.AbsListView.OnScrollListener;
import android.widget.CompoundButton.OnCheckedChangeListener;
import com.actionbarsherlock.app.SherlockActivity;

import java.util.*;

public class AppManager extends SherlockActivity implements OnCheckedChangeListener,
        OnClickListener {

    public final static String PREFS_KEY_PROXYED = "Proxyed";
    private static final int MSG_LOAD_START = 1;
    private static final int MSG_LOAD_FINISH = 2;
    final Handler handler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case MSG_LOAD_START:
                    pd = ProgressDialog.show(AppManager.this, "",
                            getString(R.string.loading), true, true);
                    break;
                case MSG_LOAD_FINISH:

                    listApps.setAdapter(adapter);

                    listApps.setOnScrollListener(new OnScrollListener() {

                        boolean visible;

                        @Override
                        public void onScroll(AbsListView view,
                                             int firstVisibleItem, int visibleItemCount,
                                             int totalItemCount) {
                            if (visible) {
                                String name = apps[firstVisibleItem].getName();
                                if (name != null && name.length() > 1)
                                    overlay.setText(apps[firstVisibleItem]
                                            .getName().substring(0, 1));
                                else
                                    overlay.setText("*");
                                overlay.setVisibility(View.VISIBLE);
                            }
                        }

                        @Override
                        public void onScrollStateChanged(AbsListView view,
                                                         int scrollState) {
                            visible = true;
                            if (scrollState == OnScrollListener.SCROLL_STATE_IDLE) {
                                overlay.setVisibility(View.INVISIBLE);
                            }
                        }
                    });

                    if (pd != null) {
                        pd.dismiss();
                        pd = null;
                    }
                    break;
            }
            super.handleMessage(msg);
        }
    };
    private ProxyedApp[] apps = null;
    private ListView listApps;
    private AppManager mAppManager;
    private TextView overlay;
    private ProgressDialog pd = null;
    private ListAdapter adapter;
    private ImageLoader dm;
    private boolean appsLoaded = false;

    public static ProxyedApp[] getProxyedApps(Context context) {

        SharedPreferences prefs = PreferenceManager
                .getDefaultSharedPreferences(context);

        String tordAppString = prefs.getString(PREFS_KEY_PROXYED, "");
        String[] tordApps;

        StringTokenizer st = new StringTokenizer(tordAppString, "|");
        tordApps = new String[st.countTokens()];
        int tordIdx = 0;
        while (st.hasMoreTokens()) {
            tordApps[tordIdx++] = st.nextToken();
        }

        Arrays.sort(tordApps);

        // else load the apps up
        PackageManager pMgr = context.getPackageManager();

        List<ApplicationInfo> lAppInfo = pMgr.getInstalledApplications(0);

        Iterator<ApplicationInfo> itAppInfo = lAppInfo.iterator();

        Vector<ProxyedApp> vectorApps = new Vector<ProxyedApp>();

        ApplicationInfo aInfo = null;

        while (itAppInfo.hasNext()) {
            aInfo = itAppInfo.next();

            // ignore system apps
            if (aInfo.uid < 10000)
                continue;

            ProxyedApp app = new ProxyedApp();

            app.setUid(aInfo.uid);

            app.setUsername(pMgr.getNameForUid(app.getUid()));

            // check if this application is allowed
            if (aInfo.packageName != null
                    && aInfo.packageName.equals("com.github.shadowsocks")) {
                app.setProxyed(true);
            } else if (Arrays.binarySearch(tordApps, app.getUsername()) >= 0) {
                app.setProxyed(true);
            } else {
                app.setProxyed(false);
            }

            vectorApps.add(app);

        }

        ProxyedApp[] apps = new ProxyedApp[vectorApps.size()];
        vectorApps.toArray(apps);
        return apps;
    }

    public void getApps(Context context) {

        SharedPreferences prefs = PreferenceManager
                .getDefaultSharedPreferences(context);

        String tordAppString = prefs.getString(PREFS_KEY_PROXYED, "");
        String[] tordApps;

        StringTokenizer st = new StringTokenizer(tordAppString, "|");
        tordApps = new String[st.countTokens()];
        int tordIdx = 0;
        while (st.hasMoreTokens()) {
            tordApps[tordIdx++] = st.nextToken();
        }

        Arrays.sort(tordApps);

        Vector<ProxyedApp> vectorApps = new Vector<ProxyedApp>();

        // else load the apps up
        PackageManager pMgr = context.getPackageManager();

        List<ApplicationInfo> lAppInfo = pMgr.getInstalledApplications(0);

        Iterator<ApplicationInfo> itAppInfo = lAppInfo.iterator();

        ApplicationInfo aInfo = null;

        while (itAppInfo.hasNext()) {
            aInfo = itAppInfo.next();

            // ignore system apps
            if (aInfo.uid < 10000)
                continue;

            if (aInfo.processName == null)
                continue;
            if (pMgr.getApplicationLabel(aInfo) == null
                    || pMgr.getApplicationLabel(aInfo).toString().equals(""))
                continue;
            if (pMgr.getApplicationIcon(aInfo) == null)
                continue;

            ProxyedApp tApp = new ProxyedApp();

            tApp.setEnabled(aInfo.enabled);
            tApp.setUid(aInfo.uid);
            tApp.setUsername(pMgr.getNameForUid(tApp.getUid()));
            tApp.setProcname(aInfo.processName);
            tApp.setName(pMgr.getApplicationLabel(aInfo).toString());

            // check if this application is allowed
            if (Arrays.binarySearch(tordApps, tApp.getUsername()) >= 0) {
                tApp.setProxyed(true);
            } else {
                tApp.setProxyed(false);
            }

            vectorApps.add(tApp);
        }

        apps = new ProxyedApp[vectorApps.size()];
        vectorApps.toArray(apps);

    }

    private void loadApps() {
        getApps(this);

        Arrays.sort(apps, new Comparator<ProxyedApp>() {
            @Override
            public int compare(ProxyedApp o1, ProxyedApp o2) {
                if (o1 == null || o2 == null || o1.getName() == null
                        || o2.getName() == null)
                    return 1;
                if (o1.isProxyed() == o2.isProxyed())
                    return o1.getName().compareTo(o2.getName());
                if (o1.isProxyed())
                    return -1;
                return 1;
            }
        });

        final LayoutInflater inflater = getLayoutInflater();

        adapter = new ArrayAdapter<ProxyedApp>(this, R.layout.layout_apps_item,
                R.id.itemtext, apps) {
            @Override
            public View getView(int position, View convertView, ViewGroup parent) {
                ListEntry entry;
                if (convertView == null) {
                    // Inflate a new view
                    convertView = inflater.inflate(R.layout.layout_apps_item,
                            parent, false);
                    entry = new ListEntry();
                    entry.icon = (ImageView) convertView
                            .findViewById(R.id.itemicon);
                    entry.box = (CheckBox) convertView
                            .findViewById(R.id.itemcheck);
                    entry.text = (TextView) convertView
                            .findViewById(R.id.itemtext);

                    entry.text.setOnClickListener(mAppManager);
                    entry.text.setOnClickListener(mAppManager);

                    convertView.setTag(entry);

                    entry.box.setOnCheckedChangeListener(mAppManager);
                } else {
                    // Convert an existing view
                    entry = (ListEntry) convertView.getTag();
                }

                final ProxyedApp app = apps[position];

                entry.icon.setTag(app.getUid());

                dm.DisplayImage(app.getUid(),
                        (Activity) convertView.getContext(), entry.icon);

                entry.text.setText(app.getName());

                final CheckBox box = entry.box;
                box.setTag(app);
                box.setChecked(app.isProxyed());

                entry.text.setTag(box);

                return convertView;
            }
        };

        appsLoaded = true;

    }

    /**
     * Called an application is check/unchecked
     */
    @Override
    public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
        final ProxyedApp app = (ProxyedApp) buttonView.getTag();
        if (app != null) {
            app.setProxyed(isChecked);
        }

        saveAppSettings(this);

    }

    @Override
    public void onClick(View v) {

        CheckBox cbox = (CheckBox) v.getTag();

        final ProxyedApp app = (ProxyedApp) cbox.getTag();
        if (app != null) {
            app.setProxyed(!app.isProxyed());
            cbox.setChecked(app.isProxyed());
        }

        saveAppSettings(this);

    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        this.setContentView(R.layout.layout_apps);

        this.dm = ImageLoaderFactory.getImageLoader(this);

        this.overlay = (TextView) View.inflate(this, R.layout.overlay, null);
        getWindowManager()
                .addView(
                        overlay,
                        new WindowManager.LayoutParams(
                                LayoutParams.WRAP_CONTENT,
                                LayoutParams.WRAP_CONTENT,
                                WindowManager.LayoutParams.TYPE_APPLICATION,
                                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                                        | WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE,
                                PixelFormat.TRANSLUCENT));

        mAppManager = this;

    }

    @Override
    protected void onResume() {
        super.onResume();

        new Thread() {

            @Override
            public void run() {
                handler.sendEmptyMessage(MSG_LOAD_START);

                listApps = (ListView) findViewById(R.id.applistview);

                if (!appsLoaded)
                    loadApps();
                handler.sendEmptyMessage(MSG_LOAD_FINISH);
            }
        }.start();

    }

    /*
      * (non-Javadoc)
      *
      * @see android.app.Activity#onStop()
      */
    @Override
    protected void onStop() {
        super.onStop();

        // Log.d(getClass().getName(),"Exiting Preferences");
    }

    public void saveAppSettings(Context context) {
        if (apps == null)
            return;

        SharedPreferences prefs = PreferenceManager
                .getDefaultSharedPreferences(this);

        // final SharedPreferences prefs =
        // context.getSharedPreferences(PREFS_KEY, 0);

        StringBuilder tordApps = new StringBuilder();

        for (int i = 0; i < apps.length; i++) {
            if (apps[i].isProxyed()) {
                tordApps.append(apps[i].getUsername());
                tordApps.append("|");
            }
        }

        Editor edit = prefs.edit();
        edit.putString(PREFS_KEY_PROXYED, tordApps.toString());
        edit.commit();

    }

    private static class ListEntry {
        private CheckBox box;
        private TextView text;
        private ImageView icon;
    }

}
