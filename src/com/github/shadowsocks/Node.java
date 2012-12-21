package com.github.shadowsocks;

public class Node {
    static {
        System.loadLibrary("node");
    }

    public static native void exec(String cmd);
}
