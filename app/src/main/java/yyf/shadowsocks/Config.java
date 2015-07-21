package yyf.shadowsocks;

import android.os.Parcel;
import android.os.Parcelable;

public class Config implements Parcelable {

    public boolean isGlobalProxy = true;
    public boolean isGFWList = true;
    public boolean isBypassApps = false;
    public boolean isTrafficStat = false;
    public boolean isUdpDns = false;

    public String profileName = "Test";
    public String proxy = "us137.skd.so";
    public String sitekey = "H49tqPj0o7";
    //public String route = "all";

    public String encMethod = "rc4-md5";
    public String proxiedAppString = "";

    public int remotePort = 54710;
    public int localPort = 1080;

    public static final Creator<Config> CREATOR = new Creator<Config>() {
        public Config createFromParcel(Parcel in) {
            return new Config(in);
        }

        public Config[] newArray(int size) {
            return new Config[size];
        }
    };

    public Config() {

    }

    public Config(boolean isGlobalProxy, boolean isGFWList, boolean isBypassApps,
                  boolean isTrafficStat, boolean isUdpDns, String profileName, String proxy, String sitekey,
                  String encMethod, String proxiedAppString, String route, int remotePort, int localPort) {
        this.isGlobalProxy = isGlobalProxy;
        this.isGFWList = isGFWList;
        this.isBypassApps = isBypassApps;
        this.isTrafficStat = isTrafficStat;
        this.isUdpDns = isUdpDns;
        this.profileName = profileName;
        this.proxy = proxy;
        this.sitekey = sitekey;
        this.encMethod = encMethod;
        this.proxiedAppString = proxiedAppString;
        //this.route = route;
        this.remotePort = remotePort;
        this.localPort = localPort;
    }

    private Config(Parcel in) {
        readFromParcel(in);
    }

    public void readFromParcel(Parcel in) {
        isGlobalProxy = in.readInt() == 1;
        isGFWList = in.readInt() == 1;
        isBypassApps = in.readInt() == 1;
        isTrafficStat = in.readInt() == 1;
        isUdpDns = in.readInt() == 1;
        profileName = in.readString();
        proxy = in.readString();
        sitekey = in.readString();
        encMethod = in.readString();
        proxiedAppString = in.readString();
        //route = in.readString();
        remotePort = in.readInt();
        localPort = in.readInt();
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel out, int flags) {
        out.writeInt(isGlobalProxy ? 1 : 0);
        out.writeInt(isGFWList ? 1 : 0);
        out.writeInt(isBypassApps ? 1 : 0);
        out.writeInt(isTrafficStat ? 1 : 0);
        out.writeInt(isUdpDns ? 1 : 0);
        out.writeString(profileName);
        out.writeString(proxy);
        out.writeString(sitekey);
        out.writeString(encMethod);
        out.writeString(proxiedAppString);
        //out.writeString(route);
        out.writeInt(remotePort);
        out.writeInt(localPort);
    }
}
