### Troubleshooting

Cannot connect to server:

1. Stop battery saver if it's active;
2. Check your config;
3. Wipe app data.

Crash: [Submit an issue](https://github.com/shadowsocks/shadowsocks-android/issues/new) with logcat attached, or submit a crash report to Google Play. Then, try wiping app data.

### How to create a widget and/or switch profile based on network connectivity?

Use [Tasker](http://tasker.dinglisch.net/) integration.

### How to add QR code from local gallery?

Scan it with a third-party scanner like [QuickMark Barcode Scanner](https://play.google.com/store/apps/details?id=tw.com.quickmark) and click the `ss` url.


### Why is NAT mode deprecated?

1. Requiring ROOT permission;
2. No IPv6 support;
3. No UDP relay support.

### How to remove the exclamation mark when using VPN mode?

The exclamation mark in the Wi-Fi/cellular icon appears because the system fails to connect to portal server (defaults to `clients3.google.com`) without VPN connection. To remove it, follow the instructions in [this article](https://www.noisyfox.cn/45.html). (in Simplified Chinese)

### Why is my ROM not supported?

1. Some ROM has broken VPNService implementation, especially for IPv6;
2. Some ROM has aggressive (or called broken) background service killing policy;
3. If you have Xposed framework and/or battery saver apps, it's likely that this app wouldn't work well with these either.

* Fixes for MIUI: [#772](https://github.com/shadowsocks/shadowsocks-android/issues/772)
* Fixes for Huawei: [#1091 (comment)](https://github.com/shadowsocks/shadowsocks-android/issues/1091#issuecomment-276949836)
* Related to Xposed: [#1414](https://github.com/shadowsocks/shadowsocks-android/issues/1414)
* Samsung and/or Brevent: [#1410](https://github.com/shadowsocks/shadowsocks-android/issues/1410)
* Don't install this app on SD card because of permission issues: [#1124 (comment)](https://github.com/shadowsocks/shadowsocks-android/issues/1124#issuecomment-307556453)

### How to pause Shadowsocks service?

* For Android 7.0+: Use quick switch tile in Quick Settings;
* Use Tasker integration;
* Add a profile with per-app proxy enabled for Shadowsocks only, bypass mode off.

### Why does Shadowsocks consume so much battery on Android 5.0+?

As Shadowsocks takes over the whole device network, any battery used by network activities from other apps are also counted as those from Shadowsocks. So, the battery usage of Shadowsocks equals to the sum of all the network activities of your device. Shadowsocks itself is a totally I/O bound application on modern Android devices, which is expected not to consume any notable battery.

So if you notice a significant increase in battery usage after you use Shadowsocks, it's most likely caused by other apps. For example, Google Play services can consume more battery after being able to connecting to Google, etc.

More details: https://kb.adguard.com/en/android/solving-problems/battery

### It works fine under Wi-Fi but can't connect through cellular data?

Allow this app to consume background data in app settings.

### Why Camera permission is required on devices below Android 6.0?

To scan the QR code through the integrated QR scanner.

By the way, upgrade your Android system already.

### How to use Transproxy mode?

1. Install [AFWall+](https://github.com/ukanth/afwall);
2. Set custom script:
```sh
IP6TABLES=/system/bin/ip6tables
IPTABLES=/system/bin/iptables
ULIMIT=/system/bin/ulimit
SHADOWSOCKS_UID=`dumpsys package com.github.shadowsocks | grep userId | cut -d= -f2 - | cut -d' ' -f1 -`
PORT_DNS=5450
PORT_TRANSPROXY=8200
$ULIMIT -n 4096
$IP6TABLES -F
$IP6TABLES -A INPUT -j DROP
$IP6TABLES -A OUTPUT -j DROP
$IPTABLES -t nat -F OUTPUT
$IPTABLES -t nat -A OUTPUT -o lo -j RETURN
$IPTABLES -t nat -A OUTPUT -d 127.0.0.1 -j RETURN
$IPTABLES -t nat -A OUTPUT -m owner --uid-owner $SHADOWSOCKS_UID -j RETURN
$IPTABLES -t nat -A OUTPUT -p tcp --dport 53 -j DNAT --to-destination 127.0.0.1:$PORT_DNS
$IPTABLES -t nat -A OUTPUT -p udp --dport 53 -j DNAT --to-destination 127.0.0.1:$PORT_DNS
$IPTABLES -t nat -A OUTPUT -p tcp -j DNAT --to-destination 127.0.0.1:$PORT_TRANSPROXY
$IPTABLES -t nat -A OUTPUT -p udp -j DNAT --to-destination 127.0.0.1:$PORT_TRANSPROXY
```
3. Set custom shutdown script:
```sh
IP6TABLES=/system/bin/ip6tables
IPTABLES=/system/bin/iptables
$IPTABLES -t nat -F OUTPUT
$IP6TABLES -F
```
4. Make sure to allow traffic for Shadowsocks;
5. Start Shadowsocks transproxy service and enable firewall.
