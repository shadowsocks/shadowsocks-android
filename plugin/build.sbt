enablePlugins(AndroidLib)
android.useSupportVectors

name := "plugin"
version := "0.0.4"

pomExtra in Global := {
  <url>https://github.com/shadowsocks/shadowsocks-android</url>
    <licenses>
      <license>
        <name>GPLv3</name>
        <url>https://www.gnu.org/licenses/gpl-3.0.html</url>
      </license>
    </licenses>
    <scm>
      <connection>scm:git:git://github.com/shadowsocks/shadowsocks-android.git</connection>
      <developerConnection>scm:git:git@github.com:shadowsocks/shadowsocks-android.git</developerConnection>
      <url>github.com/shadowsocks/shadowsocks-android</url>
    </scm>
    <developers>
      <developer>
        <id>madeye</id>
        <name>Max Lv</name>
        <url>https://github.com/madeye</url>
      </developer>
      <developer>
        <id>Mygod</id>
        <name>Mygod Studio</name>
        <url>https://github.com/Mygod</url>
      </developer>
    </developers>
}
