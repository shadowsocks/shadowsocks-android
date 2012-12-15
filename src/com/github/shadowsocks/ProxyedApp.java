package com.github.shadowsocks;

public class ProxyedApp {

    private boolean enabled;
    private int uid;
    private String username;
    private String procname;
    private String name;

    private boolean proxyed = false;

    /**
     * @return the name
     */
    public String getName() {
        return name;
    }

    /**
     * @return the procname
     */
    public String getProcname() {
        return procname;
    }

    /**
     * @return the uid
     */
    public int getUid() {
        return uid;
    }

    /**
     * @return the username
     */
    public String getUsername() {
        return username;
    }

    /**
     * @return the enabled
     */
    public boolean isEnabled() {
        return enabled;
    }

    /**
     * @return the proxyed
     */
    public boolean isProxyed() {
        return proxyed;
    }

    /**
     * @param enabled the enabled to set
     */
    public void setEnabled(boolean enabled) {
        this.enabled = enabled;
    }

    /**
     * @param name the name to set
     */
    public void setName(String name) {
        this.name = name;
    }

    /**
     * @param procname the procname to set
     */
    public void setProcname(String procname) {
        this.procname = procname;
    }

    /**
     * @param proxyed the proxyed to set
     */
    public void setProxyed(boolean proxyed) {
        this.proxyed = proxyed;
    }

    /**
     * @param uid the uid to set
     */
    public void setUid(int uid) {
        this.uid = uid;
    }

    /**
     * @param username the username to set
     */
    public void setUsername(String username) {
        this.username = username;
    }
}