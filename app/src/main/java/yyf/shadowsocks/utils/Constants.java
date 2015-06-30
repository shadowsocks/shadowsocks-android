package yyf.shadowsocks.utils;

/**
 * Created by yyf on 2015/6/18.
 */
public class Constants {
    public static class Executable {
        public static String REDSOCKS = "redsocks";
        public static String PDNSD = "pdnsd";
        public static String SS_LOCAL = "ss-local";
        public static String SS_TUNNEL = "ss-tunnel";
        public static String IPTABLES = "iptables";
        public static String TUN2SOCKS = "tun2socks";
    }

    public enum Msg {CONNECT_FINISH,CONNECT_SUCCESS,CONNECT_FAIL,VPN_ERROR};

    public static class Path {
        public static String BASE = "/data/data/yyf.shadowsocks/";
    }

    public static class Key {
        public static String profileId = "profileId";
        public static String profileName = "profileName";

        public static String proxied = "Proxyed";

        public static String isNAT = "isNAT";
        public static String isRoot = "isRoot";
        public static String status = "status";
        public static String proxyedApps = "proxyedApps";
        public static String route = "route";

        public static String isRunning = "isRunning";
        public static String isAutoConnect = "isAutoConnect";

        public static String isGlobalProxy = "isGlobalProxy";
        public static String isGFWList = "isGFWList";
        public static String isBypassApps = "isBypassApps";
        public static String isTrafficStat = "isTrafficStat";
        public static String isUdpDns = "isUdpDns";

        public static String proxy = "proxy";
        public static String sitekey = "sitekey";
        public static String encMethod = "encMethod";
        public static String remotePort = "remotePort";
        public static String localPort = "port";
    }

    public static class Scheme {
        public static String APP = "app://";
        public static String PROFILE = "profile://";
        public static String SS = "ss";
    }

    public enum Mode {NAT,VPN };

    public enum State {INIT,CONNECTING,CONNECTED,STOPPING,STOPPED,}

    public static class Action {
        public static String SERVICE = "yyf.shadowsocks.SERVICE";
        public static String CLOSE = "yyf.shadowsocks.CLOSE";
        public static String UPDATE_FRAGMENT = "yyf.shadowsocks.ACTION_UPDATE_FRAGMENT";
        public static String UPDATE_PREFS = "yyf.shadowsocks.ACTION_UPDATE_PREFS";
    }

    public static class Route {
        public static String ALL = "all";
        public static String BYPASS_LAN = "bypass-lan";
        public static String BYPASS_CHN = "bypass-china";
    }
}
