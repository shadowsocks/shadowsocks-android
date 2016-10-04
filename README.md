# 敬告:这个工程仅仅是当初将ss for android的功能用java改写的而已,仅仅是调通了而已,没有实际意义!
# shadowsocks-android-pro
A Shadowsocks client for Android


## Shadowsocks for Android

A [shadowsocks](http://shadowsocks.org) client for Android, written in Java.


正在写.......要用的下正规的这个
[![Google Play](http://developer.android.com/images/brand/en_generic_rgb_wo_45.png)](https://play.google.com/store/apps/details?id=com.github.shadowsocks)


### Build环境

* JDK 1.7+
* Android SDK 
* Android NDK 
* Android studio 或者 Gradle

* Set environment variable `ANDROID_HOME` PS:最好
* Set environment variable `ANDROID_NDK_HOME` PS:可以在工程内设置
* Invoke the building like this

```bash
    git submodule update --init
```


#### BUILD on Android Studio

* 进行一上Build
* Make Project
* copy /app/src/main/libs to /app/src/main/assets
* delete /app/src/main/libs/jnisystem.so
* Build Project

### LICENSE

Copyright (C) 2015 Yangyaofei <yangyaofei@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
