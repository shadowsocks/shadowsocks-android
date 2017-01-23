package com.github.shadowsocks.plugin;

/**
 * The contract between the plugin provider and host. Contains definitions for the supported actions, extras, etc.
 *
 * This class is written in Java to keep Java interoperability.
 *
 * @author Mygod
 */
public final class PluginContract {
    private PluginContract() { }

    /**
     * ContentProvider Action: Used for NativePluginProvider.
     *
     * Constant Value: "com.github.shadowsocks.plugin.ACTION_NATIVE_PLUGIN"
     */
    public static final String ACTION_NATIVE_PLUGIN = "com.github.shadowsocks.plugin.ACTION_NATIVE_PLUGIN";

    /**
     * Activity Action: Used for ConfigurationActivity.
     *
     * Constant Value: "com.github.shadowsocks.plugin.ACTION_CONFIGURE"
     */
    public static final String ACTION_CONFIGURE = "com.github.shadowsocks.plugin.ACTION_CONFIGURE";
    /**
     * Activity Action: Used for HelpActivity or HelpCallback.
     *
     * Constant Value: "com.github.shadowsocks.plugin.ACTION_HELP"
     */
    public static final String ACTION_HELP = "com.github.shadowsocks.plugin.ACTION_HELP";

    /**
     * The lookup key for a string that provides the plugin entry binary.
     *
     * Example: "/data/data/com.github.shadowsocks.plugin.obfs_local/lib/libobfs-local.so"
     *
     * Constant Value: "com.github.shadowsocks.plugin.EXTRA_ENTRY"
     */
    public static final String EXTRA_ENTRY = "com.github.shadowsocks.plugin.EXTRA_ENTRY";
    /**
     * The lookup key for a string that provides the options as a string.
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
     * The metadata key to retrieve plugin id. Required for plugins.
     *
     * Constant Value: "com.github.shadowsocks.plugin.id"
     */
    public static final String METADATA_KEY_ID = "com.github.shadowsocks.plugin.id";
    /**
     * The metadata key to retrieve default configuration. Default value is empty.
     *
     * Constant Value: "com.github.shadowsocks.plugin.default_config"
     */
    public static final String METADATA_KEY_DEFAULT_CONFIG = "com.github.shadowsocks.plugin.default_config";

    public static final String METHOD_GET_EXECUTABLE = "shadowsocks:getExecutable";

    /** ConfigurationActivity result: fallback to manual edit mode. */
    public static final int RESULT_FALLBACK = 1;

    /**
     * Relative to the file to be copied. This column is required.
     *
     * Example: "kcptun", "doc/help.txt"
     *
     * Type: String
     */
    public static final String COLUMN_PATH = "path";
    /**
     * File mode bits. Default value is "644".
     *
     * Example: "755"
     *
     * Type: String
     */
    public static final String COLUMN_MODE = "mode";

    /**
     * The scheme for general plugin actions.
     */
    public static final String SCHEME = "plugin";
    /**
     * The authority for general plugin actions.
     */
    public static final String AUTHORITY = "com.github.shadowsocks";
}
