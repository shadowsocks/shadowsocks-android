package com.github.shadowsocks.plugin;

/**
 * This class provides String constants that will be used in plugin interfaces.
 *
 * This class is written in Java to keep Java interoperability.
 *
 * @author Mygod
 */
public final class PluginInterface {
    private PluginInterface() { }

    /**
     * Should be a NativePluginProvider.
     *
     * Constant Value: "com.github.shadowsocks.plugin.CATEGORY_NATIVE_PLUGIN"
     */
    public static final String CATEGORY_NATIVE_PLUGIN = "com.github.shadowsocks.plugin.CATEGORY_NATIVE_PLUGIN";

    /**
     * The lookup key for a string that provides the whole command as a string.
     *
     * Example: "obfs=http;obfs-host=www.baidu.com"
     *
     * Constant Value: "com.github.shadowsocks.plugin.EXTRA_OPTIONS"
     */
    public static final String EXTRA_OPTIONS = "com.github.shadowsocks.plugin.EXTRA_OPTIONS";
    /**
     * The lookup key for a CharSequence that provides user relevant help message.
     *
     * Example: "obfs=<http|tls>            Enable obfuscating: HTTP or TLS (Experimental).
     *           obfs-host=<host_name>      Hostname for obfuscating (Experimental)."
     *
     * Constant Value: "com.github.shadowsocks.plugin.EXTRA_HELP_MESSAGE"
     */
    public static final String EXTRA_HELP_MESSAGE = "com.github.shadowsocks.plugin.EXTRA_HELP_MESSAGE";

    /**
     * The metadata key to retrieve default configuration. Default value is empty.
     *
     * Constant Value: "com.github.shadowsocks.plugin.default_config"
     */
    public static final String METADATA_KEY_DEFAULT_CONFIG = "com.github.shadowsocks.plugin.default_config";

    /**
     * Relative to the file to be copied. This column is required.
     *
     * Example: "kcptun", "doc/help.txt"
     *
     * Type: String
     */
    public static final String COLUMN_PATH = "path";

    /**
     * Authority to use for native plugin ContentProvider.
     *
     * @param pluginId Plugin ID.
     * @return com.github.shadowsocks.plugin.$PLUGIN_ID
     */
    public static String getAuthority(String pluginId) {
        return "com.github.shadowsocks.plugin." + pluginId;
    }

    /**
     * Activity Action: Used for ConfigurationActivity.
     *
     * @param pluginId Plugin ID.
     * @return com.github.shadowsocks.plugin.$PLUGIN_ID.ACTION_CONFIGURE
     */
    public static String ACTION_CONFIGURE(String pluginId) {
        return getAuthority(pluginId) + ".ACTION_CONFIGURE";
    }
    /**
     * Activity Action: Used for HelpActivity or HelpCallback.
     *
     * @param pluginId Plugin ID.
     * @return com.github.shadowsocks.plugin.$PLUGIN_ID.ACTION_HELP
     */
    public static String ACTION_HELP(String pluginId) {
        return getAuthority(pluginId) + ".ACTION_HELP";
    }
}
