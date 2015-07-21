package yyf.shadowsocks.utils;

import android.os.Build;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.PrintWriter;



/**
 * Created by yyf on 2015/6/26.
 */
public class ConfigUtils {
    static public String SHADOWSOCKS = "{\"server\": \"%s\", \"server_port\": %d, \"local_port\": %d, \"password\": \"%s\", \"method\":\"%s\", \"timeout\": %d}";

    static public String PDNSD_LOCAL =
            "global { \n" +
                    "perm_cache = 2048;\n" +
                    "cache_dir = \"/data/data/yyf.shadowsocks\";" +
                    "server_ip = %s;" +
                    "server_port = %d;" +
                    "query_method = tcp_only;" +
                    "run_ipv4 = on;" +
                    "min_ttl = 15m;" +
                    "max_ttl = 1w;" +
                    "timeout = 10;" +
                    "daemon = on;" +
                    "pid_file = %s;" +
                    "}" +
                    "server {" +
                    "label = \"local\";" +
                    "ip = 127.0.0.1;" +
                    "port = %d;" +
                    "reject = ::/0;" +
                    "reject_policy = negate;" +
                    "reject_recursively = on;" +
                    "timeout = 5;" +
                    "}" +
                    "rr {" +
                    "name=localhost;" +
                    "reverse=on;" +
                    "a=127.0.0.1;" +
                    "owner=localhost;" +
                    "soa=localhost,root.localhost,42,86400,900,86400,86400;" +
                    "}";

    static public String PDNSD_BYPASS =
            "global {" +
                    "perm_cache = 2048;" +
                    "cache_dir = \"/data/data/yyf.shadowsocks\";" +
                    "server_ip = %s;" +
                    "server_port = %d;" +
                    "query_method = tcp_only;" +
                    "run_ipv4 = on;" +
                    "min_ttl = 15m;" +
                    "max_ttl = 1w;" +
                    "timeout = 10;" +
                    "daemon = on;" +
                    "pid_file = \"%s\";" +
                    "}" +
                    "server {" +
                    "label = \"china-servers\";" +
                    "ip = 114.114.114.114, 223.5.5.5;" +
                    "uptest = none;" +
                    "preset = on;" +
                    "include = %s;" +
                    "policy = excluded;" +
                    "timeout = 2;" +
                    "}" +
                    "server {" +
                    "label = \"local-server\";" +
                    "ip = 127.0.0.1;" +
                    "uptest = none;" +
                    "preset = on;" +
                    "port = %d;" +
                    "timeout = 5;" +
                    "}" +
                    "rr {" +
                    "name=localhost;" +
                    "reverse=on;" +
                    "a=127.0.0.1;" +
                    "owner=localhost;" +
                    "soa=localhost,root.localhost,42,86400,900,86400,86400;" +
                    "}";


    static public String PDNSD_DIRECT =
            "global {\n" +
            " perm_cache = 2048;\n" +
            " cache_dir = \"/data/yyf.shadowsocks\";\n" +
            " server_ip = %s;\n" +
            " server_port = %d;\n" +
            " query_method = tcp_only;\n" +
            " run_ipv4 = on;\n" +
            " min_ttl = 15m;\n" +
            " max_ttl = 1w;\n" +
            " timeout = 10;\n" +
            " daemon = on;\n" +
            " pid_file = \"%s\";\n" +
            "}\n" +
            "server {\n" +
            " label = \"china-servers\";\n" +
            " ip = 223.5.5.5, 114.114.114.114;\n" +
            " timeout = 2;\n" +
            " reject = %s;\n" +
            " reject_policy = fail;\n" +
            " reject_recursively = on;\n" +
            " exclude = %s;\n" +
            " policy = included;\n" +
            " uptest = none;\n" +
            " preset = on;\n" +
            "}\n" +
            "server {\n" +
            " label = \"local-server\";\n" +
            " ip = 127.0.0.1;\n" +
            " port = %d;\n" +
            " timeout = 3;\n" +
            "}\n" +
            "\n" +
            "rr {\n" +
            " name=localhost;\n" +
            " reverse=on;\n" +
            " a=127.0.0.1;\n" +
            " owner=localhost;\n" +
            " soa=localhost,root.localhost,42,86400,900,86400,86400;\n" +
            "}\n";

    public static PrintWriter printToFile(File f) {
        PrintWriter p = null;
        try {
            p = new PrintWriter(f);
            return p;
        } catch (FileNotFoundException e) {
            e.printStackTrace();
            return null;
        }
    }
    public static boolean isLollipopOrAbove(){
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            return true;
        } else {
            return false;
        }
    }
        /*TODO 需要自定义Application修改此代码
        void refresh(Context context) {
        val holder = context.getApplicationContext.asInstanceOf[ShadowsocksApplication].containerHolder
        if (holder != null) holder.refresh()
        }

        def getRejectList(context: Context, app: ShadowsocksApplication): String = {
        val default = context.getString(R.string.reject)
        try {
        val container = app.containerHolder.getContainer
        val update = container.getString("reject")
        if (update == null ""update.isEmpty) default else update
        } catch {
        case ex: Exception => default
}
        }

        def getBlackList(context: Context, app: ShadowsocksApplication): String = {
        val default = context.getString(R.string.black_list)
        try {
        val container = app.containerHolder.getContainer
        val update = container.getString("black_list")
        if (update == null ""update.isEmpty) default else update
        } catch {
        case ex: Exception => default
}
        }
        *
        Config getPublicConfig(Context context,Container container,Config config){
        String url = container.getString("proxy_url");
        val sig = Utils.getSignature(context);
        val list = HttpRequest
        .post(url)
        .connectTimeout(2000)
        .readTimeout(2000)
        .send("sig="+sig)
        .body
        val proxies = util.Random.shuffle(list.split('"').toSeq).toSeq
        val proxy = proxies(0).split(':')

        val host = proxy(0).trim
        val port = proxy(1).trim.toInt
        val password = proxy(2).trim
        val method = proxy(3).trim

        new Config(config.isGlobalProxy, config.isGFWList, config.isBypassApps, config.isTrafficStat,
        config.isUdpDns, config.profileName, host, password, method, config.proxiedAppString, config.route, port,
        config.localPort)
        }

        def load(settings: SharedPreferences): Config = {
        val isGlobalProxy = settings.getBoolean(Key.isGlobalProxy, false)
        val isGFWList = settings.getBoolean(Key.isGFWList, false)
        val isBypassApps = settings.getBoolean(Key.isBypassApps, false)
        val isTrafficStat = settings.getBoolean(Key.isTrafficStat, false)
        val isUdpDns = settings.getBoolean(Key.isUdpDns, false)

        val profileName = settings.getString(Key.profileName, "default")
        val proxy = settings.getString(Key.proxy, "127.0.0.1")
        val sitekey = settings.getString(Key.sitekey, "default")
        val encMethod = settings.getString(Key.encMethod, "table")
        val route = settings.getString(Key.route, "all")

        val remotePort: Int = try {
        settings.getString(Key.remotePort, "1984").toInt
        } catch {
        case ex: NumberFormatException =>
        1984
        }
        val localPort: Int = try {
        settings.getString(Key.localPort, "1984").toInt
        } catch {
        case ex: NumberFormatException =>
        1984
        }
        val proxiedAppString = settings.getString(Key.proxied, "")

        new Config(isGlobalProxy, isGFWList, isBypassApps, isTrafficStat, isUdpDns, profileName, proxy,
        sitekey, encMethod, proxiedAppString, route, remotePort, localPort)
        }

*/

}