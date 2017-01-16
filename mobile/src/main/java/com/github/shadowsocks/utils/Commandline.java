package com.github.shadowsocks.utils;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.StringTokenizer;

/**
 * Commandline objects help handling command lines specifying processes to
 * execute.
 *
 * The class can be used to define a command line as nested elements or as a
 * helper to define a command line by an application.
 * <p>
 * <code>
 * &lt;someelement&gt;<br>
 * &nbsp;&nbsp;&lt;acommandline executable="/executable/to/run"&gt;<br>
 * &nbsp;&nbsp;&nbsp;&nbsp;&lt;argument value="argument 1" /&gt;<br>
 * &nbsp;&nbsp;&nbsp;&nbsp;&lt;argument line="argument_1 argument_2 argument_3" /&gt;<br>
 * &nbsp;&nbsp;&nbsp;&nbsp;&lt;argument value="argument 4" /&gt;<br>
 * &nbsp;&nbsp;&lt;/acommandline&gt;<br>
 * &lt;/someelement&gt;<br>
 * </code>
 *
 * Based on: https://github.com/apache/ant/blob/588ce1f/src/main/org/apache/tools/ant/types/Commandline.java
 *
 * Adds support for escape character '\'.
 *
 * @author Mygod
 */
public final class Commandline {
    private Commandline() { }

    /**
     * Quote the parts of the given array in way that makes them
     * usable as command line arguments.
     * @param args the list of arguments to quote.
     * @return empty string for null or no command, else every argument split
     * by spaces and quoted by quoting rules.
     */
    public static String toString(Iterable<String> args) {
        // empty path return empty string
        if (args == null) {
            return "";
        }
        // path containing one or more elements
        final StringBuilder result = new StringBuilder();
        for (String arg : args) {
            if (result.length() > 0) result.append(' ');
            for (int j = 0; j < arg.length(); ++j) {
                char ch = arg.charAt(j);
                switch (ch) {
                    case ' ': case '\\': case '"': case '\'': result.append('\\');  // intentionally no break
                    default: result.append(ch);
                }
            }
        }
        return result.toString();
    }
    /**
     * Quote the parts of the given array in way that makes them
     * usable as command line arguments.
     * @param args the list of arguments to quote.
     * @return empty string for null or no command, else every argument split
     * by spaces and quoted by quoting rules.
     */
    public static String toString(String[] args) {
        return toString(Arrays.asList(args));   // thanks to Java, arrays aren't iterable
    }

    /**
     * Crack a command line.
     * @param toProcess the command line to process.
     * @return the command line broken into strings.
     * An empty or null toProcess parameter results in a zero sized array.
     */
    public static String[] translateCommandline(String toProcess) {
        if (toProcess == null || toProcess.length() == 0) {
            //no command? no string
            return new String[0];
        }
        // parse with a simple finite state machine

        final int normal = 0;
        final int inQuote = 1;
        final int inDoubleQuote = 2;
        int state = normal;
        final StringTokenizer tok = new StringTokenizer(toProcess, "\\\"\' ", true);
        final ArrayList<String> result = new ArrayList<>();
        final StringBuilder current = new StringBuilder();
        boolean lastTokenHasBeenQuoted = false;
        boolean lastTokenIsSlash = false;

        while (tok.hasMoreTokens()) {
            String nextTok = tok.nextToken();
            switch (state) {
                case inQuote:
                    if ("\'".equals(nextTok)) {
                        lastTokenHasBeenQuoted = true;
                        state = normal;
                    } else {
                        current.append(nextTok);
                    }
                    break;
                case inDoubleQuote:
                    if ("\"".equals(nextTok)) {
                        if (lastTokenIsSlash) {
                            current.append(nextTok);
                            lastTokenIsSlash = false;
                        } else {
                            lastTokenHasBeenQuoted = true;
                            state = normal;
                        }
                    } else if ("\\".equals(nextTok)) {
                        if (lastTokenIsSlash) {
                            current.append(nextTok);
                            lastTokenIsSlash = false;
                        } else lastTokenIsSlash = true;
                    } else {
                        if (lastTokenIsSlash) {
                            current.append("\\");   // unescaped
                            lastTokenIsSlash = false;
                        }
                        current.append(nextTok);
                    }
                    break;
                default:
                    if (lastTokenIsSlash) {
                        current.append(nextTok);
                        lastTokenIsSlash = false;
                    } else if ("\\".equals(nextTok)) lastTokenIsSlash = true; else if ("\'".equals(nextTok)) {
                        state = inQuote;
                    } else if ("\"".equals(nextTok)) {
                        state = inDoubleQuote;
                    } else if (" ".equals(nextTok)) {
                        if (lastTokenHasBeenQuoted || current.length() != 0) {
                            result.add(current.toString());
                            current.setLength(0);
                        }
                    } else {
                        current.append(nextTok);
                    }
                    lastTokenHasBeenQuoted = false;
                    break;
            }
        }
        if (lastTokenHasBeenQuoted || current.length() != 0) {
            result.add(current.toString());
        }
        if (state == inQuote || state == inDoubleQuote) {
            throw new IllegalArgumentException("unbalanced quotes in " + toProcess);
        }
        if (lastTokenIsSlash) throw new IllegalArgumentException("escape character following nothing in " + toProcess);
        return result.toArray(new String[result.size()]);
    }
}
