* 1.2.0:
  * New helper class `AlertDialogFragment` for creating `AlertDialog` that persists through configuration changes;
  * Dependency update: `com.google.android.material:material:1.1.0-alpha03`.
* 1.1.0:
  * Having control characters in plugin options is no longer allowed.
    If this breaks your plugin, you are doing it wrong.
  * New helper method: `PluginOptions.putWithDefault`.
* 1.0.0:
  * BREAKING CHANGE: Plugins developed using this version and forward require shadowsocks-android 4.6.5 or higher.
  * `PathProvider` now takes `Int` instead of `String` for file modes;
  * Refactor to AndroidX;
  * No longer depends on preference libraries.
* 0.1.1:
  * Rewritten in Kotlin;
  * Fix assert not working;
  * Min API 21;
  * Update support library version to 27.1.1.
* 0.0.4:
  * Enlarge text size of number pickers;
  * Update support library version to 26.0.0.
* 0.0.3:
  * Update support library version to 25.2.0.
* 0.0.2:
  * Add `getOrDefault` to `PluginOptions`;
  * Update support library version to 25.1.1.
* 0.0.1: Initial release.
