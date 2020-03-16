# Documentation for JSON format

shadowsocks-android accepts processing Shadowsocks configs through JSON files.
This documentation is targeted towards Shadowsocks server maintainers, who might need to distribute server configs through subscription JSON files, which is supported since [v5.0.0](https://github.com/shadowsocks/shadowsocks-android/releases/tag/v5.0.0).

## `Profile` JSON object

In general, shadowsocks-android respects [the specification at shadowsocks.org](https://shadowsocks.org/en/config/quick-guide.html).
A JSON object is recognized as a `Profile` (i.e. a valid server config), if all of the following required fields are present and valid:

* `server`
* `server_port`
* `password`
* `method`

These fields have the same meaning as specified by shadowsocks.org.
The specification at shadowsocks.org additionally specifies two additional fields, which is not used by shadowsocks-android:

* `local_port`: This field is configured globally through "SOCKS5 proxy port".
* `timeout`: This field is hardcoded to 600 in shadowsocks-android (however, it is only used as a write timeout, so long idle connections like GMS heartbeat are allowed).

Additionally, shadowsocks-android accepts the following optional fields:

* `plugin`: shadowsocks-android plugin ID or [alias](https://github.com/shadowsocks/shadowsocks-android/pull/2431).
* `plugin_opts`: Plugin options [as specified in shadowsocks.org](https://shadowsocks.org/en/spec/Plugin.html).
* `remarks`: "Profile Name". (also used by [shadowsocks-windows](https://github.com/shadowsocks/shadowsocks-windows))
* `udp_fallback`: "UDP Fallback", should be a `Profile` JSON object or null.

The following optional fields are "Feature Settings" in shadowsocks-android, if omitted, their values will be copied from currently selected profile:

* `route`: "Route", should be one of `all`, `bypass-lan`, `bypass-china`, `bypass-lan-china`, `gfwlist`, `china-list`, `custom-rules`.
* `ipv6`: "IPv6 Route", Boolean.
* `proxy_apps`: "Apps VPN mode", is a JSON object with the following fields:
  - `enabled`: Boolean.
  - `bypass`: Boolean, whether the app specified should be bypassed or proxied.
  - `android_list`: An array of strings, specifying a list of Android app [package names](https://developer.android.com/studio/build/application-id).
* `udpdns`: "Send DNS over UDP", Boolean.

`Profile` objects can have additional fields, which will be ignored by shadowsocks-android.


## Parsing/Importing JSON

shadowsocks-android imports JSON using the following logic, which should support all reasonable formats of JSON files containing `Profile`s, including `gui-config.json` used by shadowsocks-windows.

1. On input a JSON file, try to recognize it as a `Profile`.
2. If input is an object and all the required fields of a `Profile` is present, return the parsed `Profile`.
3. Otherwise if input is an object, recursively search for `Profile`s in each field and return all found `Profile`s.
4. Otherwise if input is an array, recursively search for `Profile`s for each element and return all found `Profile`s.
5. Conclude that there is no `Profile` in input.

In general, subscription JSON file can be in any structure, as long as they contain the `Profile` JSON objects recognized by the above procedure.


## Exporting JSON

The easiest way to write JSON files is to configure them in shadowsocks-android and export the JSON file using the app.
The output will be a JSON file consisting of a JSON array with all the `Profile`s.


## Subscription update behavior

In shadowsocks-android, `Profile` is indexed by the triple `(server, server_port, remarks)`.
If two `Profile`s share the exact same triple, they will be treated as the exact same `Profile`, despite possibly having different plugins, passwords, or even encryption methods.

When doing subscription updates,

* If an old `Profile` shares the same triple as a new `Profile`, the following field in the old `Profile` will be updated: `password`, `method`, `plugin`, `plugin_opts`, `udp_fallback`.
  This ensures that user configured feature settings and traffic stats are preserved.
* If two `Profile` across all the subscriptions share the exact same triple, the behavior is undefined.
  Please avoid this if possible.


## Appendix: Sample exported JSON file

```json
[
  {
    "server": "198.199.101.152",
    "server_port": 8388,
    "password": "u1rRWTssNv0p",
    "method": "aes-256-cfb",
    "remarks": "Example 1"
  },
  {
    "server": "198.199.101.12",
    "server_port": 8388,
    "password": "u1rRWTssNv0p",
    "method": "aes-256-cfb",
    "plugin": "v2ray",
    "plugin_opts": "host=www.example.com",
    "remarks": "Example 2",
    "route": "bypass-lan-china",
    "remote_dns": "dns.google",
    "ipv6": true,
    "metered": false,
    "proxy_apps": {
      "enabled": true,
      "bypass": true,
      "android_list": [
        "com.eg.android.AlipayGphone",
        "com.wudaokou.hippo",
        "com.zhihu.android"
      ]
    },
    "udpdns": false
  },
]
```
