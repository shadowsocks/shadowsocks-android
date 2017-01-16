package com.github.shadowsocks.plugin;

import android.text.TextUtils;

import java.util.HashMap;
import java.util.StringTokenizer;

/**
 * Helper class for processing plugin options.
 *
 * Based on: https://github.com/apache/ant/blob/588ce1f/src/main/org/apache/tools/ant/types/Commandline.java
 *
 * @author Mygod
 */
public final class PluginOptions extends HashMap<String, String> {
    public PluginOptions() {
        super();
    }
    public PluginOptions(int initialCapacity) {
        super(initialCapacity);
    }
    public PluginOptions(int initialCapacity, float loadFactor) {
        super(initialCapacity, loadFactor);
    }

    private PluginOptions(String options, boolean parseId) throws IllegalArgumentException {
        if (TextUtils.isEmpty(options)) return;
        final StringTokenizer tokenizer = new StringTokenizer(options, "\\=;", true);
        final StringBuilder current = new StringBuilder();
        String key = null;
        while (tokenizer.hasMoreTokens()) {
            String nextToken = tokenizer.nextToken();
            if ("\\".equals(nextToken)) current.append(tokenizer.nextToken());
            else if ("=".equals(nextToken)) {
                if (key != null) throw new IllegalArgumentException("Duplicate keys in " + options);
                key = current.toString();
                current.setLength(0);
            } else if (";".equals(nextToken))
                if (key != null) {
                    put(key, current.toString());
                    key = null;
                } else if (parseId) {
                    id = current.toString();
                    parseId = false;
                } else {
                    put(current.toString(), null);
                    current.setLength(0);
                }
        }
    }
    public PluginOptions(String options) throws IllegalArgumentException {
        this(options, true);
    }
    public PluginOptions(String id, String options) throws IllegalArgumentException {
        this(options, false);
        this.id = id;
    }

    public String id;

    private static void append(StringBuilder result, String str) {
        for (int i = 0; i < str.length(); ++i) {
            char ch = str.charAt(i);
            switch (ch) {
                case '\\': case '=': case ';': result.append('\\'); // intentionally no break
                default: result.append(ch);
            }
        }
    }
    public String toString(boolean trimId) {
        final StringBuilder result = new StringBuilder();
        if (!trimId) if (TextUtils.isEmpty(id)) return ""; else append(result, id);
        for (Entry<String, String> entry : entrySet()) {
            if (result.length() > 0) result.append(';');
            append(result, entry.getKey());
            String value = entry.getValue();
            if (value != null) {
                result.append('=');
                append(result, value);
            }
        }
        return result.toString();
    }
    @Override
    public String toString() {
        return toString(true);
    }
}
