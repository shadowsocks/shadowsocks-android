/* 
 *  Copyright 2010 Brave New Software Project, Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

package com.github.shadowsocks.utils;

import android.annotation.SuppressLint;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.annotation.TargetApi;
import android.util.Log;
import android.net.ConnectivityManager;
import android.net.LinkProperties;
import android.os.Build;

import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;
import java.util.ArrayList;
import java.util.Collection;
import java.net.InetAddress;

public class Dns {

    public static String getDnsResolver(Context context) throws Exception {
        Collection<InetAddress> dnsResolvers = getDnsResolvers(context);
        if (dnsResolvers.isEmpty()) {
            throw new Exception("Couldn't find an active DNS resolver");
        }
        String dnsResolver = dnsResolvers.iterator().next().toString();
        if (dnsResolver.startsWith("/")) {
            dnsResolver = dnsResolver.substring(1);
        }
        return dnsResolver;
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private static Collection<InetAddress> getDnsResolvers(Context context) throws Exception {
        ArrayList<InetAddress> addresses = new ArrayList<InetAddress>();
        ConnectivityManager connectivityManager =
            (ConnectivityManager)context.getSystemService(Context.CONNECTIVITY_SERVICE);
        Class<?> LinkPropertiesClass = Class.forName("android.net.LinkProperties");
        Method getActiveLinkPropertiesMethod = ConnectivityManager.class.getMethod("getActiveLinkProperties", new Class []{});
        Object linkProperties = getActiveLinkPropertiesMethod.invoke(connectivityManager);
        if (linkProperties != null) {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
                Method getDnsesMethod = LinkPropertiesClass.getMethod("getDnses", new Class []{});
                Collection<?> dnses = (Collection<?>)getDnsesMethod.invoke(linkProperties);
                for (Object dns : dnses) {
                    addresses.add((InetAddress)dns);
                }
            } else {
                for (InetAddress dns : ((LinkProperties)linkProperties).getDnsServers()) {
                    addresses.add(dns);
                }
            }
        }

        return addresses;
    }
}
