package yyf.shadowsocks.service;


import android.app.NotificationManager;
import android.content.BroadcastReceiver;
import android.content.Intent;
import android.net.VpnService;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.util.Log;


import java.io.File;

import yyf.shadowsocks.BuildConfig;
import yyf.shadowsocks.utils.ConfigUtils;
import yyf.shadowsocks.utils.Console;
import yyf.shadowsocks.utils.Constants;

/**
 * Created by yyf on 2015/6/18.
 */
public class ShadowsocksVpnService extends BaseService {

    String TAG = "ShadowsocksVpnService";
    int VPN_MTU = 1500;
    String PRIVATE_VLAN = "26.26.26.%s";

    ParcelFileDescriptor conn = null;
    NotificationManager notificationManager = null;
    BroadcastReceiver receiver = null;
    //Array<ProxiedApp> apps = null; 功能去掉...
    //Config config = null;

    //private ShadowsocksApplication application = This.getApplication();//<ShadowsocksApplication>;
    boolean isByass() {
        //       val info = net.getInfo;
        //     info.isInRange(config.proxy);
        //TODO :完成此函数  参数 SubnetUtils net
        return false;
    }

    boolean isPrivateA(int a) {
        if (a == 10 || a == 192 || a == 172) {
            return true;
        } else {
            return false;
        }
    }

    boolean isPrivateB(int a, int b) {
        if (a == 10 || (a == 192 && b == 168) || (a == 172 && b >= 16 && b < 32)) {
            return true;
        } else {
            return false;
        }
    }

    public void startShadowsocksDaemon() {
       /* String[] acl =  getResources().getStringArray(R.array.private_route);
        //ConfigUtils.printToFile(new File(Constants.Path.BASE + "acl.list"))(p => {
                //acl.foreach(item => p.println(item))
        //路由表写入文件
        PrintWriter printWriter = ConfigUtils.printToFile(new File(Constants.Path.BASE + "acl.list"));
        for (int i = 0; i < acl.length; i++)
            printWriter.println(acl[i]);
        /* 是否全局判断  先按照非全局走
        if (!Constants.Route.ALL.equals(config.route)){
            switch(config.route){
                case Constants.Route.BYPASS_LAN:
                    getResources().getStringArray(R.array.private_route);
                case Constants.Route.BYPASS_CHN:
                    getResources().getStringArray(R.array.chn_route_full);
            }
            ConfigUtils.printToFile(new File(Constants.Path.BASE + "acl.list"))(p => {
                    acl.foreach(item => p.println(item))
            })
        }*/
        //读取配置并写入文件
        /*String conf = "hehe";
        //Config conf;//= new Config();//ConfigUtils.SHADOWSOCKS.formatLocal(Locale.ENGLISH, config.proxy, config.remotePort, config.localPort,
                       // config.sitekey, config.encMethod, 10)

        ConfigUtils.printToFile(new File(Constants.Path.BASE + "ss-local-vpn.conf")).println(conf);

        String[] cmd = {
            Constants.Path.BASE+"ss-local","-u",
            "-b" , "127.0.0.1",
            "-t","600","-c",
            Constants.Path.BASE + "ss-local-vpn.conf","-f ",
            Constants.Path.BASE + "ss-local-vpn.pid"
        };
        if ( !Constants.Route.ALL.equals(config.route)) {
            List<String> list = Arrays.asList(cmd);
            list.add(" --acl");
            list.add(Constants.Path.BASE + "acl.list");
            cmd = list.toArray(new String[0]);
        }

        if (BuildConfig.DEBUG)
            Log.d(TAG, cmd.toString());
            //Log.d(TAG, cmd.mkString(" "));
        Console.runCommand(Console.mkCMD(cmd));
        //Console.runCommand(cmd.mkString(" "));
        */
    }

    public void startDnsTunnel() {
        /*val conf = ConfigUtils
                .SHADOWSOCKS.formatLocal(Locale.ENGLISH, config.proxy, config.remotePort, 8163,
                        config.sitekey, config.encMethod, 10)
        */
        String conf = "hehe";
        ConfigUtils.printToFile(new File(Constants.Path.BASE + "ss-tunnel-vpn.conf")).println(conf);
        String[] cmd = {
                Constants.Path.BASE + "ss-tunnel"
                , "-u"
                , "-t", "10"
                , "-b", "127.0.0.1"
                , "-l", "8163"
                , "-L", "8.8.8.8:53"
                , "-c", Constants.Path.BASE + "ss-tunnel-vpn.conf"
                , "-f", Constants.Path.BASE + "ss-tunnel-vpn.pid"};

        if (BuildConfig.DEBUG)
            Log.d(TAG, cmd.toString());
        //Log.d(TAG, cmd.mkString(" "))
        Console.runCommand(Console.mkCMD(cmd));
    }

    public void startDnsDaemon() {
        String conf = "";
        //val conf = {////////////////////*****************/
        /*if (config.route == Constants.Route.BYPASS_CHN) {
            String reject = getResources().getString(R.string.reject);
            String blackList = getResources().getString(R.string.black_list);
            conf = ConfigUtils.PDNSD_DIRECT.format(Locale.ENGLISH, "0.0.0.0", 8153,
                    Constants.Path.BASE + "pdnsd-vpn.pid", reject, blackList, 8163);
        } else {
            conf = ConfigUtils.PDNSD_LOCAL.format(Locale.ENGLISH, "0.0.0.0", 8153,
                    Constants.Path.BASE + "pdnsd-vpn.pid", 8163);
        }
        ConfigUtils.printToFile(new File(Constants.Path.BASE + "pdnsd-vpn.conf")).println(conf);

        String cmd = Constants.Path.BASE + "pdnsd -c " + Constants.Path.BASE + "pdnsd-vpn.conf";

        if (BuildConfig.DEBUG)
            Log.d(TAG, cmd);
        Console.runCommand(cmd);
        */
    }

    /*
    String getVersionName{
        var version: String = null
        try {
            val pi: PackageInfo = getPackageManager.getPackageInfo(getPackageName, 0)
            version = pi.versionName
        } catch {
            case e: PackageManager.NameNotFoundException =>
                version = "Package name not found"
        }
        version
    }
    */
    void startVpn() {

        /*Builder builder = new Builder();
        builder
                .setSession(config.profileName)
                .setMtu(VPN_MTU)
                .addAddress(PRIVATE_VLAN.format(Locale.ENGLISH, "1"), 24)
                .addDnsServer("8.8.8.8");
        Log.v("ss-vpn","startVpn");
        /*if (Utils.isLollipopOrAbove) {

            builder.allowFamily(android.system.OsConstants.AF_INET6);

            if (!config.isGlobalProxy) {
                val apps = AppManager.getProxiedApps(this, config.proxiedAppString)
                val pkgSet: mutable.HashSet[String] = new mutable.HashSet[String]
                for (app <- apps) {
                    if (app.proxied) {
                        pkgSet.add(app.packageName)
                    }
                }
                for (pkg <- pkgSet) {
                    if (!config.isBypassApps) {
                        builder.addAllowedApplication(pkg)
                    } else {
                        builder.addDisallowedApplication(pkg)
                    }
                }

                if (config.isBypassApps) {
                    builder.addDisallowedApplication(this.getPackageName)
                }
            } else {
                builder.addDisallowedApplication(this.getPackageName)
            }
        }
        */
        /*if (InetAddressUtils.isIPv6Address(config.proxy)) {
            builder.addRoute("0.0.0.0", 0);
        } else {
            if (!Utils.isLollipopOrAbove) {
                config.route match {
                    case Constants.Route.BYPASS_LAN =>
                        for (i <- 1 to 223) {
                        if (i != 26 && i != 127) {
                            val addr = i.toString + ".0.0.0"
                            val cidr = addr + "/8"
                            val net = new SubnetUtils(cidr)

                            if (!isByass(net) && !isPrivateA(i)) {
                                builder.addRoute(addr, 8)
                            } else {
                                for (j <- 0 to 255) {
                                    val subAddr = i.toString + "." + j.toString + ".0.0"
                                    val subCidr = subAddr + "/16"
                                    val subNet = new SubnetUtils(subCidr)
                                    if (!isByass(subNet) && !isPrivateB(i, j)) {
                                        builder.addRoute(subAddr, 16)
                                    }
                                }
                            }
                        }
                    }
                    case Route.BYPASS_CHN =>
                        val list = {
                        if (Build.VERSION.SDK_INT == Build.VERSION_CODES.KITKAT) {
                            getResources.getStringArray(R.array.simple_route)
                        } else {
                            getResources.getStringArray(R.array.gfw_route)
                        }
                        }
                        list.foreach(cidr => {
                                val net = new SubnetUtils(cidr)
                        if (!isByass(net)) {
                            val addr = cidr.split('/')
                            builder.addRoute(addr(0), addr(1).toInt)
                        }
                        })
                    case Route.ALL =>
                        for (i <- 1 to 223) {
                        if (i != 26 && i != 127) {
                            val addr = i.toString + ".0.0.0"
                            val cidr = addr + "/8"
                            val net = new SubnetUtils(cidr)

                            if (!isByass(net)) {
                                builder.addRoute(addr, 8)
                            } else {
                                for (j <- 0 to 255) {
                                    val subAddr = i.toString + "." + j.toString + ".0.0"
                                    val subCidr = subAddr + "/16"
                                    val subNet = new SubnetUtils(subCidr)
                                    if (!isByass(subNet)) {
                                        builder.addRoute(subAddr, 16)
                                    }
                                }
                            }
                        }
                    }
                }
            } else {
                if (config.route == Route.ALL) {
                    builder.addRoute("0.0.0.0", 0)
                } else {
                    val privateList = getResources.getStringArray(R.array.bypass_private_route)
                    privateList.foreach(cidr => {
                            val addr = cidr.split('/')
                            builder.addRoute(addr(0), addr(1).toInt)
                    })
                }
            }
        }

        builder.addRoute("8.8.0.0", 16)

        try {
            conn = builder.establish()
        } catch {
            case ex: IllegalStateException =>
                changeState(State.STOPPED, ex.getMessage)
                conn = null
            case ex: Exception => conn = null
        }

        if (conn == null) {
            stopRunner()
            return
        }

        val fd = conn.getFd

        var cmd = (Path.BASE +
                "tun2socks --netif-ipaddr %s "
                + "--netif-netmask 255.255.255.0 "
                + "--socks-server-addr 127.0.0.1:%d "
                + "--tunfd %d "
                + "--tunmtu %d "
                + "--loglevel 3 "
                + "--pid %stun2socks-vpn.pid")
                .formatLocal(Locale.ENGLISH, PRIVATE_VLAN.formatLocal(Locale.ENGLISH, "2"), config.localPort, fd, VPN_MTU, Path.BASE)

        if (config.isUdpDns)
            cmd += " --enable-udprelay"
        else
            cmd += " --dnsgw %s:8153".formatLocal(Locale.ENGLISH, PRIVATE_VLAN.formatLocal(Locale.ENGLISH, "1"))

        if (Utils.isLollipopOrAbove) {
            cmd += " --fake-proc"
        }

        if (BuildConfig.DEBUG) Log.d(TAG, cmd)

        System.exec(cmd)
        */
    }

    /**
     * Called when the activity is first created.
     */
    public boolean handleConnection() {
        /*startShadowsocksDaemon();
        if (!config.isUdpDns) {
            startDnsDaemon();
            startDnsTunnel();
        }
        startVpn();
        */
        return true;
    }

    @Override
    public IBinder onBind(Intent intent) {
        String action = intent.getAction();
        if (VpnService.SERVICE_INTERFACE == action) {
            return super.onBind(intent);
        } else if (Constants.Action.SERVICE == action) {

            return null;//binder;
        }
        return null;
    }

    @Override
    public void onCreate() {
        super.onCreate();
        Log.v("ss-vpn", "oncreate");
        //ConfigUtils.refresh(this)
        //notificationManager = getSystemService(Context.NOTIFICATION_SERVICE)
        //.asInstanceOf[NotificationManager]
    }

    @Override
    public void onRevoke() {
        //stopRunner();
        Log.v("ss-vpn", "oncreate");
    }

    public void killProcesses() {
    /*for (task <- Array("ss-local", "ss-tunnel", "pdnsd", "tun2socks")) {
    try {
    val pid = scala.io.Source.fromFile(Constants.Path.BASE + task + "-vpn.pid").mkString.trim.toInt
    Process.killProcess(pid)
    } catch {
    case e: Throwable => Log.e(TAG, "unable to kill " + task)
    }
    }*/
    }

    /*
    @Override
    public void stopBackgroundService() {
        stopSelf();
    }
    */
    /*@Override
    public void startRunner(Config c) {
        /*
        config = c;

        // ensure the VPNService is prepared
        if (VpnService.prepare(this) != null) {
            Intent i = new Intent(this);
            i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            startActivity(i);
            return;
        }

        // send event
        application.tracker.send(new HitBuilders.EventBuilder()
                .setCategory(TAG)
                .setAction("start")
                .setLabel(getVersionName)
                .build())

        // register close receiver
        IntentFilter filter = new IntentFilter();
        filter.addAction(Intent.ACTION_SHUTDOWN);
        receiver = new BroadcastReceiver {
            def onReceive(p1: Context, p2: Intent) {
                Toast.makeText(p1, R.string.stopping, Toast.LENGTH_SHORT)
                stopRunner()
            }
        }
        registerReceiver(receiver, filter);

        changeState(Constants.State.CONNECTING)

        spawn {
            if (config.proxy == "198.199.101.152") {
                val holder = getApplication.asInstanceOf[ShadowsocksApplication].containerHolder
                try {
                    config = ConfigUtils.getPublicConfig(getBaseContext, holder.getContainer, config)
                } catch {
                    case ex: Exception =>
                        changeState(State.STOPPED, getString(R.string.service_failed))
                        stopRunner()
                        config = null
                }
            }

            if (config != null) {

                // reset the context
                killProcesses()

                // Resolve the server address
                var resolved: Boolean = false
                if (!InetAddressUtils.isIPv4Address(config.proxy) &&
                        !InetAddressUtils.isIPv6Address(config.proxy)) {
                    Utils.resolve(config.proxy, enableIPv6 = true) match {
                        case Some(addr) =>
                            config.proxy = addr
                            resolved = true
                        case None => resolved = false
                    }
                } else {
                    resolved = true
                }

                if (resolved && handleConnection) {
                    changeState(State.CONNECTED)
                } else {
                    changeState(State.STOPPED, getString(R.string.service_failed))
                    stopRunner()
                }
            }
        }

    }*/
    /*
    @Override
    public void stopRunner() {
    /*
        // channge the state
        changeState(Constants.State.STOPPING);

        // send event
        application.tracker.send(new EventBuilder()
                .setCategory(TAG)
                .setAction("stop")
                .setLabel(getVersionName())
                .build());

        // reset VPN
        killProcesses();

        // close connections
        if (conn != null) {
            try {
                conn.close();
                conn = null;
            }catch(IOException e){
                e.printStackTrace();
            }
        }

        // stop the service if no callback registered
        if (getCallbackCount() == 0) {
            stopSelf();
        }

        // clean up the context
        if (receiver != null) {
            unregisterReceiver(receiver);
            receiver = null;
        }

        // channge the state
        changeState(Constants.State.STOPPED);
        *
    }
    */
    /*
    @Override
    public Constants.Mode getServiceMode() {
        return Constants.Mode.VPN;
    }
    */
    /*
    @Override
    public String getTag() {
        return TAG;
    }
    */
    /*
    @Override
    public Context getContext() {
        return getBaseContext();
    }
    */
}