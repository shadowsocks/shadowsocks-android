package yyf.shadowsocks;

import android.app.Activity;
import android.content.Intent;
import android.net.VpnService;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.FragmentPagerAdapter;
import android.support.v4.app.FragmentTransaction;
import android.support.v4.view.ViewPager;
import android.support.v7.app.ActionBar;
import android.support.v7.app.ActionBarActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import java.util.Locale;

import yyf.shadowsocks.service.ShadowsocksVpnService;
import yyf.shadowsocks.utils.ServerListDB;
import yyf.shadowsocks.preferences.JsonPreference;


public class MainActivity extends ActionBarActivity implements ActionBar.TabListener {
    SectionsPagerAdapter mSectionsPagerAdapter;
    ViewPager mainPager;//主界面 base
    //vpnservice
    public static int REQUEST_CONNECT = 1;
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);

        /* Set up the action bar.*/
        final ActionBar actionBar = getSupportActionBar();
        actionBar.setNavigationMode(ActionBar.NAVIGATION_MODE_TABS);
        // Create the adapter that will return a fragment for each of the three
        // primary sections of the activity.
        mSectionsPagerAdapter = new SectionsPagerAdapter(getSupportFragmentManager());
        // Set up the ViewPager with the sections adapter.
        mainPager = (ViewPager) findViewById(R.id.pager);
        mainPager.setAdapter(mSectionsPagerAdapter);
        // When swiping between different sections, select the corresponding
        // tab. We can also use ActionBar.Tab#select() to do this if we have
        // a reference to the Tab.
        mainPager.setOnPageChangeListener(new ViewPager.SimpleOnPageChangeListener() {
            @Override
            public void onPageSelected(int position) {
                actionBar.setSelectedNavigationItem(position);
            }
        });
        // For each of the sections in the app, add a tab to the action bar.
        for (int i = 0; i < mSectionsPagerAdapter.getCount(); i++) {
            // Create a tab with text corresponding to the page title defined by
            // the adapter. Also specify this Activity object, which implements
            // the TabListener interface, as the callback (listener) for when
            // this tab is selected.
            actionBar.addTab(
                    actionBar.newTab()
                            .setText(mSectionsPagerAdapter.getPageTitle(i))
                            .setTabListener(this));
        }
        /*Set up the action barSet up the action bar END*/

    }
    void prepareStartService() {
        //showProgress(getString(R.string.connecting));
        //if (isVpnEnabled) {
        Intent intent = VpnService.prepare(this);
        if (intent != null) {
            startActivityForResult(intent, MainActivity.REQUEST_CONNECT);
            Log.v("ss-vpn","startActivityForResult");
        } else {
            onActivityResult(MainActivity.REQUEST_CONNECT, Activity.RESULT_OK, null);
            Log.v("ss-vpn", "onActivityResult");
        }
        //cancelStart()



    }
    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        if (result == RESULT_OK) {
            Intent intent = new Intent(this, ShadowsocksVpnService.class);
            Log.v("ss-vpn","StartVpnService");
            startService(intent);
        }
    }
    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.menu_main, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        switch(item.getItemId()){
            case R.id.action_settings:
                //设置
                Log.v("ss-log","settings");
                return true;
            case R.id.action_swipe:
                //ss开关
                Log.v("ss-log", "swipe");
                Log.v("ss-vpn","------ start VPN -----");
                prepareStartService();
                //startService(new Intent(this,ShadowsocksVpnService.class));
                return true;
            case R.id.loadFromFile:
                //加载JSON
                ServerListDB sldb = new ServerListDB(this);
                sldb.deleteAll();
                Log.v("ss-log", "deleteDB");
                sldb.add(JsonPreference.getServerPerferenc());
                Log.v("ss-log", "addDB");
                sldb.close();
                Log.v("ss-log", "loadFromFile");
            default:
                return super.onOptionsItemSelected(item);
        }
    }

    @Override
    public void onTabSelected(ActionBar.Tab tab, FragmentTransaction fragmentTransaction) {

    }

    @Override
    public void onTabUnselected(ActionBar.Tab tab, FragmentTransaction fragmentTransaction) {

    }

    @Override
    public void onTabReselected(ActionBar.Tab tab, FragmentTransaction fragmentTransaction) {

    }

    public class SectionsPagerAdapter extends FragmentPagerAdapter {

        public SectionsPagerAdapter(FragmentManager fm) {
            super(fm);
        }

        @Override
        public Fragment getItem(int position) {
            // getItem is called to instantiate the fragment for the given page.
            // Return a PlaceholderFragment (defined as a static inner class below).
            /*switch(position){
                case 0:
                    return new ServerListFragment();
                case 1:
                    return new DaemonManagerFragment();
                case 2:
                    return new LogCatFragment();
                default:
                    return new ServerListFragment();
            }*/
            return TabFragment.newInstance(position + 1);

        }

        @Override
        public int getCount() {
            // Show 3 total pages.
            return 3;
        }

        @Override
        public CharSequence getPageTitle(int position) {
            Locale l = Locale.getDefault();
            switch (position) {
                case 0:
                    return ("1").toUpperCase(l);
                case 1:
                    return ("2").toUpperCase(l);
                case 2:
                    return ("3").toUpperCase(l);
            }
            return null;
        }
    }
}


