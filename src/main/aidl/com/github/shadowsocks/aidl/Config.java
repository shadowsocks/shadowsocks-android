package com.github.shadowsocks.aidl;

import android.os.Parcel;
import android.os.Parcelable;

public class Config implements Parcelable {

  public boolean isProxyApps = false;
  public boolean isBypassApps = false;
  public boolean isUdpDns = false;
  public boolean isAuth = false;
  public boolean isIpv6 = false;

  public String profileName = "Untitled";
  public String proxy = "127.0.0.1";
  public String sitekey = "null";
  public String route = "all";

  public String encMethod = "rc4";
  public String proxiedAppString = "";

  public int remotePort = 1984;
  public int localPort = 1080;

  public int profileId = 0;

  public static final Parcelable.Creator<Config> CREATOR = new Parcelable.Creator<Config>() {
    public Config createFromParcel(Parcel in) {
      return new Config(in);
    }

    public Config[] newArray(int size) {
      return new Config[size];
    }
  };

  public Config(boolean isProxyApps, boolean isBypassApps,
      boolean isUdpDns, boolean isAuth, boolean isIpv6, String profileName, String proxy, String sitekey,
      String encMethod, String proxiedAppString, String route, int remotePort, int localPort, int profileId) {
    this.isProxyApps = isProxyApps;
    this.isBypassApps = isBypassApps;
    this.isUdpDns = isUdpDns;
    this.isAuth = isAuth;
    this.isIpv6 = isIpv6;
    this.profileName = profileName;
    this.proxy = proxy;
    this.sitekey = sitekey;
    this.encMethod = encMethod;
    this.proxiedAppString = proxiedAppString;
    this.route = route;
    this.remotePort = remotePort;
    this.localPort = localPort;
    this.profileId = profileId;
  }

  private Config(Parcel in) {
    readFromParcel(in);
  }

  public void readFromParcel(Parcel in) {
    isProxyApps = in.readInt() == 1;
    isBypassApps = in.readInt() == 1;
    isUdpDns = in.readInt() == 1;
    isAuth = in.readInt() == 1;
    isIpv6 = in.readInt() == 1;
    profileName = in.readString();
    proxy = in.readString();
    sitekey = in.readString();
    encMethod = in.readString();
    proxiedAppString = in.readString();
    route = in.readString();
    remotePort = in.readInt();
    localPort = in.readInt();
    profileId = in.readInt();
  }

  @Override public int describeContents() {
    return 0;
  }

  @Override public void writeToParcel(Parcel out, int flags) {
    out.writeInt(isProxyApps ? 1 : 0);
    out.writeInt(isBypassApps ? 1 : 0);
    out.writeInt(isUdpDns ? 1 : 0);
    out.writeInt(isAuth ? 1 : 0);
    out.writeInt(isIpv6 ? 1 : 0);
    out.writeString(profileName);
    out.writeString(proxy);
    out.writeString(sitekey);
    out.writeString(encMethod);
    out.writeString(proxiedAppString);
    out.writeString(route);
    out.writeInt(remotePort);
    out.writeInt(localPort);
    out.writeInt(profileId);
  }
}
