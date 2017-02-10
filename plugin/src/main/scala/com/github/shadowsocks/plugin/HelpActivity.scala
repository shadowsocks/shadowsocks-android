package com.github.shadowsocks.plugin

/**
  * Base class for a help activity. A help activity is started when user taps help when configuring options for your
  * plugin. To create a help activity, just extend this class, and add it to your manifest like this:
  *
  * <pre class="prettyprint">&lt;manifest&gt;
  *    ...
  *    &lt;application&gt;
  *        ...
  *        &lt;activity android:name=".HelpActivity"&gt;
  *            &lt;intent-filter&gt;
  *                &lt;action android:name="com.github.shadowsocks.plugin.ACTION_HELP"/&gt;
  *                &lt;category android:name="android.intent.category.DEFAULT"/&gt;
  *                &lt;data android:scheme="plugin"
  *                         android:host="com.github.shadowsocks"
  *                         android:path="/$PLUGIN_ID"/&gt;
  *            &lt;/intent-filter&gt;
  *        &lt;/activity&gt;
  *        ...
  *    &lt;/application&gt;
  *&lt;/manifest&gt;</pre>
  *
  * @author Mygod
  */
trait HelpActivity extends OptionsCapableActivity {
  // HelpActivity can choose to ignore options
  override protected def onInitializePluginOptions(options: PluginOptions): Unit = ()
}
