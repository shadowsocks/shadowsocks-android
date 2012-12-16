## Shadowsocks for Android

A shadowsocks client for Android, powered by amazing node.js.

## TRAVIS CI STATUS

[![Build Status](https://secure.travis-ci.org/madeye/shadowsocks-android.png)](http://travis-ci.org/madeye/shadowsocks-android)

[Nightly Builds](http://buildbot.sinaapp.com)

## PREREQUISITES

* JDK 1.6+
* Maven 3.0.3+
* Android SDK r17+
* Android NDK r8+
* Local maven dependencies
  Use Maven Android SDK Deployer to install all android related dependencies.
  ```bash
  git clone https://github.com/mosabua/maven-android-sdk-deployer.git 
  pushd maven-android-sdk-deployer
  export ANDROID_HOME=/path/to/android/sdk
  mvn install -P 4.1
  popd
  ```
* Build native dependecies
  ```bash
  ndk-build
  cp libs/armeabi/pdnsd assets/
  ```

## BUILD

* Create your key following the instructions at
http://developer.android.com/guide/publishing/app-signing.html#cert

* Create a profile in your settings.xml file in ~/.m2 like this

```xml
  <settings>
    <profiles>
      <profile>
        <activation>
          <activeByDefault>true</activeByDefault>
        </activation>
        <properties>
          <sign.keystore>/absolute/path/to/your.keystore</sign.keystore>
          <sign.alias>youralias</sign.alias>
          <sign.keypass>keypass</sign.keypass>
          <sign.storepass>storepass</sign.storepass>
        </properties>
      </profile>
    </profiles>
  </settings>
```

* Invoke the building like this

```bash
  mvn clean install
```
