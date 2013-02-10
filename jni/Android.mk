# Copyright (C) 2009 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
#
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LIBEVENT_SOURCES := \
	buffer.c \
	bufferevent.c bufferevent_filter.c \
	bufferevent_openssl.c bufferevent_pair.c bufferevent_ratelim.c \
	bufferevent_sock.c epoll.c \
	epoll_sub.c evdns.c event.c \
    event_tagging.c evmap.c \
	evrpc.c evthread.c \
	evthread_pthread.c evutil.c \
	evutil_rand.c http.c \
	listener.c log.c poll.c \
	select.c signal.c strlcpy.c

LOCAL_MODULE := event
LOCAL_SRC_FILES := $(addprefix libevent/, $(LIBEVENT_SOURCES))
LOCAL_CFLAGS := -O2 -g -I$(LOCAL_PATH)/libevent \
	-I$(LOCAL_PATH)/libevent/include \
	-I$(LOCAL_PATH)/openssl/include

include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)

REDSOCKS_SOURCES := base.c dnstc.c http-connect.c \
	log.c md5.c socks5.c \
	base64.c http-auth.c http-relay.c main.c \
	parser.c redsocks.c socks4.c utils.c

LOCAL_STATIC_LIBRARIES := libevent

LOCAL_MODULE := redsocks
LOCAL_SRC_FILES := $(addprefix redsocks/, $(REDSOCKS_SOURCES))
LOCAL_CFLAGS := -O2 -std=gnu99 -g -I$(LOCAL_PATH)/redsocks \
	-I$(LOCAL_PATH)/libevent/include \
	-I$(LOCAL_PATH)/libevent

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

SPDYLAY_SOURCES := spdylay_buffer.c \
	spdylay_client_cert_vector.c \
	spdylay_frame.c \
	spdylay_gzip.c \
	spdylay_helper.c \
	spdylay_map.c \
	spdylay_npn.c \
	spdylay_outbound_item.c \
	spdylay_pq.c \
	spdylay_queue.c \
	spdylay_session.c \
	spdylay_stream.c \
	spdylay_submit.c \
	spdylay_zlib.c

LOCAL_STATIC_LIBRARIES := libevent libcrypto

LOCAL_MODULE := spdylay
LOCAL_SRC_FILES := $(addprefix spdylay/, $(SPDYLAY_SOURCES))
LOCAL_CFLAGS := -O2 -g -I$(LOCAL_PATH)/spdylay \
	-I$(LOCAL_PATH)/spdylay/includes \
	-I$(LOCAL_PATH)/libevent/include \
	-I$(LOCAL_PATH)/libevent \
	-I$(LOCAL_PATH)/openssl/include

include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)

SHRPX_SOURCES := util.cc spdylay_ssl.cc \
	spdylay_config.h shrpx_config.cc \
	shrpx_listen_handler.cc \
	shrpx_client_handler.cc \
	shrpx_upstream.h shrpx_spdy_upstream.cc \
	shrpx_https_upstream.cc \
	shrpx_downstream_queue.cc \
	shrpx_downstream.cc \
	shrpx_downstream_connection.cc \
	shrpx_http_downstream_connection.cc \
	shrpx_spdy_downstream_connection.cc \
	shrpx_spdy_session.cc \
	shrpx_log.cc shrpx_http.cc \
	shrpx_io_control.cc \
	shrpx_ssl.cc shrpx_thread_event_receiver.cc \
	shrpx_worker.cc \
	shrpx_accesslog.cc http-parser/http_parser.c \
	shrpx.cc 

LOCAL_STATIC_LIBRARIES := libevent libcrypto libssl libspdylay

LOCAL_LDLIBS := -lz

LOCAL_MODULE := shrpx
LOCAL_SRC_FILES := $(addprefix shrpx/, $(SHRPX_SOURCES))
LOCAL_CFLAGS := -O2 -g -I$(LOCAL_PATH)/spdylay \
	-I$(LOCAL_PATH)/spdylay/includes \
	-I$(LOCAL_PATH)/shrpx \
	-I$(LOCAL_PATH)/libevent/include \
	-I$(LOCAL_PATH)/libevent \
	-I$(LOCAL_PATH)/openssl/include

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

OBFSPROXY_SOURCES := container.c crypt.c external.c \
	main.c managed.c network.c \
	obfs_main.c protocol.c sha256.c \
	socks.c status.c util.c \
	protocols/dummy.c protocols/obfs2.c

LOCAL_STATIC_LIBRARIES := libevent libcrypto

LOCAL_MODULE := obfsproxy
LOCAL_SRC_FILES := $(addprefix obfsproxy/, $(OBFSPROXY_SOURCES))
LOCAL_CFLAGS := -O2 -g -I$(LOCAL_PATH)/obfsproxy \
	-I$(LOCAL_PATH)/libevent/include \
	-I$(LOCAL_PATH)/libevent \
	-I$(LOCAL_PATH)/openssl/include

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

POLIPO_SOURCES := util.c event.c io.c chunk.c atom.c object.c log.c diskcache.c main.c \
	config.c local.c http.c client.c server.c auth.c tunnel.c \
	http_parse.c parse_time.c dns.c forbidden.c \
	md5.c fts_compat.c socks.c mingw.c

LOCAL_MODULE := polipo
LOCAL_SRC_FILES := $(addprefix polipo/, $(POLIPO_SOURCES))
LOCAL_CFLAGS := -O2 -g -DHAS_STDINT_H -DNO_DISK_CACHE -DNO_SYSLOG -I$(LOCAL_PATH)/polipo

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_MODULE := libev
LOCAL_CFLAGS += -O2 -DNDEBUG -DHAVE_CONFIG_H
LOCAL_SRC_FILES := \
	libev/ev.c \
	libev/event.c 

include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)

PDNSD_SOURCES  := $(wildcard $(LOCAL_PATH)/pdnsd/*.c)

LOCAL_MODULE    := pdnsd
LOCAL_SRC_FILES := $(PDNSD_SOURCES:$(LOCAL_PATH)%=%)
LOCAL_CFLAGS    := -Wall -O2 -I$(LOCAL_PATH)/pdnsd

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_MODULE    := shadowsocks 
LOCAL_SRC_FILES := shadowsocks/local.c shadowsocks/encrypt.c
LOCAL_CFLAGS    := -Wall -O2 -fno-strict-aliasing -I$(LOCAL_PATH)/libev/ -I$(LOCAL_PATH)/openssl/include

LOCAL_STATIC_LIBRARIES := libev libcrypto

LOCAL_LDLIBS := -llog

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_MODULE:= exec

LOCAL_SRC_FILES:= \
	   termExec.cpp

LOCAL_LDLIBS := -ldl -llog

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE:= system

LOCAL_SRC_FILES:= \
	   system.cpp

LOCAL_LDLIBS := -ldl -llog

include $(BUILD_SHARED_LIBRARY)

subdirs := $(addprefix $(LOCAL_PATH)/openssl/,$(addsuffix /Android.mk, \
	crypto \
	ssl \
	))

include $(subdirs)

