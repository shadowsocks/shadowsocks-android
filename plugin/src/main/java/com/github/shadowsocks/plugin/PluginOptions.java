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

    public PluginOptions(String options) throws IllegalArgumentException {
        if (TextUtils.isEmpty(options)) return;
        final StringTokenizer tokenizer = new StringTokenizer(options, "\\=;", true);
        final StringBuilder current = new StringBuilder();
        String key = null;
        boolean firstEntry = true;
        while (tokenizer.hasMoreTokens()) {
            String nextToken = tokenizer.nextToken();
            if ("\\".equals(nextToken)) current.append(tokenizer.nextToken());
            else if ("=".equals(nextToken)) {
                if (key != null) throw new IllegalArgumentException("Duplicate keys in " + options);
                key = current.toString();
                current.setLength(0);
            } else if (";".equals(nextToken)) {
                if (current.length() > 0) put(key, current.toString());
                else if (firstEntry) id = key;
                else throw new IllegalArgumentException("Value missing in " + options);
                firstEntry = false;
            }
        }
    }
    public PluginOptions(String id, String options) throws IllegalArgumentException {
        this(options);
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
        if (!trimId && !TextUtils.isEmpty(id)) append(result, id);
        for (Entry<String, String> entry : entrySet()) if (entry.getValue() != null) {
            if (result.length() > 0) result.append(';');
            append(result, entry.getKey());
            result.append('=');
            append(result, entry.getValue());
        }
        return result.toString();
    }
    @Override
    public String toString() {
        return toString(false);
    }
}
