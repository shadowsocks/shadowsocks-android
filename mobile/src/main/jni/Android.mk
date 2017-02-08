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
ROOT_PATH := $(LOCAL_PATH)

BUILD_SHARED_EXECUTABLE := $(LOCAL_PATH)/build-shared-executable.mk

########################################################
## libsodium
########################################################

include $(CLEAR_VARS)

SODIUM_SOURCE := \
	crypto_aead/chacha20poly1305/sodium/aead_chacha20poly1305.c \
	crypto_core/salsa20/ref/core_salsa20.c \
	crypto_generichash/blake2/ref/blake2b-compress-ref.c \
	crypto_generichash/blake2/ref/blake2b-ref.c \
	crypto_generichash/blake2/ref/generichash_blake2b.c \
	crypto_onetimeauth/poly1305/onetimeauth_poly1305.c \
	crypto_onetimeauth/poly1305/donna/poly1305_donna.c \
	crypto_pwhash/crypto_pwhash.c \
	crypto_pwhash/argon2/argon2-core.c \
	crypto_pwhash/argon2/argon2.c \
	crypto_pwhash/argon2/argon2-encoding.c \
	crypto_pwhash/argon2/argon2-fill-block-ref.c \
	crypto_pwhash/argon2/blake2b-long.c \
	crypto_pwhash/argon2/pwhash_argon2i.c \
	crypto_scalarmult/curve25519/scalarmult_curve25519.c \
	crypto_stream/chacha20/stream_chacha20.c \
	crypto_stream/chacha20/ref/stream_chacha20_ref.c \
	crypto_stream/salsa20/ref/stream_salsa20_ref.c \
	crypto_stream/salsa20/ref/xor_salsa20_ref.c \
	crypto_verify/16/ref/verify_16.c \
	randombytes/randombytes.c \
	randombytes/sysrandom/randombytes_sysrandom.c \
	sodium/core.c \
	sodium/runtime.c \
	sodium/utils.c \
	sodium/version.c

LOCAL_MODULE := sodium
LOCAL_CFLAGS += -O2 -I$(LOCAL_PATH)/libsodium/src/libsodium/include \
				-I$(LOCAL_PATH)/include \
				-I$(LOCAL_PATH)/include/sodium \
				-I$(LOCAL_PATH)/libsodium/src/libsodium/include/sodium \
				-DPACKAGE_NAME=\"libsodium\" -DPACKAGE_TARNAME=\"libsodium\" \
				-DPACKAGE_VERSION=\"1.0.7\" -DPACKAGE_STRING=\"libsodium\ 1.0.7\" \
				-DPACKAGE_BUGREPORT=\"https://github.com/jedisct1/libsodium/issues\" \
				-DPACKAGE_URL=\"https://github.com/jedisct1/libsodium\" \
				-DPACKAGE=\"libsodium\" -DVERSION=\"1.0.7\" -DSTDC_HEADERS=1 \
				-DHAVE_SYS_TYPES_H=1 -DHAVE_SYS_STAT_H=1 -DHAVE_STDLIB_H=1 \
				-DHAVE_STRING_H=1 -DHAVE_MEMORY_H=1 -DHAVE_STRINGS_H=1 \
				-DHAVE_INTTYPES_H=1 -DHAVE_STDINT_H=1 -DHAVE_UNISTD_H=1 \
				-D__EXTENSIONS__=1 -D_ALL_SOURCE=1 -D_GNU_SOURCE=1 \
				-D_POSIX_PTHREAD_SEMANTICS=1 -D_TANDEM_SOURCE=1 \
				-DHAVE_DLFCN_H=1 -DLT_OBJDIR=\".libs/\" \
				-DHAVE_SYS_MMAN_H=1 -DNATIVE_LITTLE_ENDIAN=1 \
				-DHAVE_WEAK_SYMBOLS=1 -DHAVE_ARC4RANDOM=1 -DHAVE_ARC4RANDOM_BUF=1 \
				-DHAVE_MLOCK=1 -DHAVE_MPROTECT=1 -DHAVE_POSIX_MEMALIGN=1

LOCAL_SRC_FILES := $(addprefix libsodium/src/libsodium/,$(SODIUM_SOURCE))

include $(BUILD_STATIC_LIBRARY)

########################################################
## libevent
########################################################

include $(CLEAR_VARS)

LIBEVENT_SOURCES := \
	buffer.c \
	bufferevent.c bufferevent_filter.c \
	bufferevent_pair.c bufferevent_ratelim.c \
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
LOCAL_CFLAGS := -O2 -I$(LOCAL_PATH)/libevent \
	-I$(LOCAL_PATH)/libevent/include \

include $(BUILD_STATIC_LIBRARY)

########################################################
## libancillary
########################################################

include $(CLEAR_VARS)

ANCILLARY_SOURCE := fd_recv.c fd_send.c

LOCAL_MODULE := libancillary
LOCAL_CFLAGS += -O2 -I$(LOCAL_PATH)/libancillary

LOCAL_SRC_FILES := $(addprefix libancillary/, $(ANCILLARY_SOURCE))

include $(BUILD_STATIC_LIBRARY)

########################################################
## libipset
########################################################

include $(CLEAR_VARS)

bdd_src = bdd/assignments.c bdd/basics.c bdd/bdd-iterator.c bdd/expanded.c \
		  		  bdd/reachable.c bdd/read.c bdd/write.c
map_src = map/allocation.c map/inspection.c map/ipv4_map.c map/ipv6_map.c \
		  		  map/storage.c
set_src = set/allocation.c set/inspection.c set/ipv4_set.c set/ipv6_set.c \
		  		  set/iterator.c set/storage.c

IPSET_SOURCE := general.c $(bdd_src) $(map_src) $(set_src)

LOCAL_MODULE := libipset
LOCAL_CFLAGS += -O2 -I$(LOCAL_PATH)/shadowsocks-libev/libipset/include \
				-I$(LOCAL_PATH)/shadowsocks-libev/libcork/include

LOCAL_SRC_FILES := $(addprefix shadowsocks-libev/libipset/src/libipset/,$(IPSET_SOURCE))

include $(BUILD_STATIC_LIBRARY)

########################################################
## libcork
########################################################

include $(CLEAR_VARS)

cli_src := cli/commands.c
core_src := core/allocator.c core/error.c core/gc.c \
			core/hash.c core/ip-address.c core/mempool.c \
			core/timestamp.c core/u128.c
ds_src := ds/array.c ds/bitset.c ds/buffer.c ds/dllist.c \
		  ds/file-stream.c ds/hash-table.c ds/managed-buffer.c \
		  ds/ring-buffer.c ds/slice.c
posix_src := posix/directory-walker.c posix/env.c posix/exec.c \
			 posix/files.c posix/process.c posix/subprocess.c
pthreads_src := pthreads/thread.c

CORK_SOURCE := $(cli_src) $(core_src) $(ds_src) $(posix_src) $(pthreads_src)

LOCAL_MODULE := libcork
LOCAL_CFLAGS += -O2 -I$(LOCAL_PATH)/shadowsocks-libev/libcork/include \
				-DCORK_API=CORK_LOCAL

LOCAL_SRC_FILES := $(addprefix shadowsocks-libev/libcork/src/libcork/,$(CORK_SOURCE))

include $(BUILD_STATIC_LIBRARY)

########################################################
## libudns
########################################################

include $(CLEAR_VARS)

UDNS_SOURCES := udns_dn.c udns_dntosp.c udns_parse.c udns_resolver.c udns_init.c \
	udns_misc.c udns_XtoX.c \
	udns_rr_a.c udns_rr_ptr.c udns_rr_mx.c udns_rr_txt.c udns_bl.c \
	udns_rr_srv.c udns_rr_naptr.c udns_codes.c udns_jran.c

LOCAL_MODULE := libudns
LOCAL_CFLAGS += -O2 -I$(LOCAL_PATH)/libudns \
				-DHAVE_DECL_INET_NTOP

LOCAL_SRC_FILES := $(addprefix libudns/,$(UDNS_SOURCES))

include $(BUILD_STATIC_LIBRARY)

########################################################
## libev 
########################################################

include $(CLEAR_VARS)

LOCAL_MODULE := libev
LOCAL_CFLAGS += -O2 -DNDEBUG -DHAVE_CONFIG_H \
				-I$(LOCAL_PATH)/include/libev
LOCAL_SRC_FILES := \
	libev/ev.c \
	libev/event.c 

include $(BUILD_STATIC_LIBRARY)

########################################################
## redsocks
########################################################

include $(CLEAR_VARS)

REDSOCKS_SOURCES := base.c http-connect.c \
	log.c md5.c socks5.c \
	base64.c http-auth.c http-relay.c main.c \
	parser.c redsocks.c socks4.c utils.c

LOCAL_STATIC_LIBRARIES := libevent

LOCAL_MODULE := redsocks
LOCAL_SRC_FILES := $(addprefix redsocks/, $(REDSOCKS_SOURCES)) 
LOCAL_CFLAGS := -O2 -std=gnu99 -DUSE_IPTABLES \
	-I$(LOCAL_PATH)/redsocks \
	-I$(LOCAL_PATH)/libevent/include \
	-I$(LOCAL_PATH)/libevent

include $(BUILD_SHARED_EXECUTABLE)

########################################################
## pdnsd
########################################################

include $(CLEAR_VARS)

PDNSD_SOURCES  := $(wildcard $(LOCAL_PATH)/pdnsd/src/*.c)

LOCAL_MODULE    := pdnsd
LOCAL_SRC_FILES := $(PDNSD_SOURCES:$(LOCAL_PATH)/%=%)
LOCAL_CFLAGS    := -DANDROID -Wall -O2 -I$(LOCAL_PATH)/pdnsd \
				   -I$(LOCAL_PATH)/include/pdnsd -I$(LOCAL_PATH)/libancillary
LOCAL_STATIC_LIBRARIES := libancillary
LOCAL_LDLIBS := -llog

include $(BUILD_SHARED_EXECUTABLE)

########################################################
## shadowsocks-libev local
########################################################

include $(CLEAR_VARS)

SHADOWSOCKS_SOURCES := local.c \
	cache.c udprelay.c utils.c netutils.c json.c jconf.c \
	acl.c http.c tls.c rule.c \
	crypto.c aead.c stream.c base64.c \
	plugin.c \
	android.c

LOCAL_MODULE    := ss-local
LOCAL_SRC_FILES := $(addprefix shadowsocks-libev/src/, $(SHADOWSOCKS_SOURCES))
LOCAL_CFLAGS    := -Wall -O2 -fno-strict-aliasing -DMODULE_LOCAL \
					-DUSE_CRYPTO_MBEDTLS -DANDROID -DHAVE_CONFIG_H \
					-DCONNECT_IN_PROGRESS=EINPROGRESS \
					-I$(LOCAL_PATH)/include/shadowsocks-libev \
					-I$(LOCAL_PATH)/include \
					-I$(LOCAL_PATH)/libancillary \
					-I$(LOCAL_PATH)/mbedtls/include  \
					-I$(LOCAL_PATH)/pcre \
					-I$(LOCAL_PATH)/libudns \
					-I$(LOCAL_PATH)/shadowsocks-libev/libcork/include \
					-I$(LOCAL_PATH)/libsodium/src/libsodium/include \
					-I$(LOCAL_PATH)/libsodium/src/libsodium/include/sodium \
					-I$(LOCAL_PATH)/shadowsocks-libev/libipset/include \
					-I$(LOCAL_PATH)/libev

LOCAL_STATIC_LIBRARIES := libev libmbedtls libipset libcork libudns \
	libsodium libancillary libpcre

LOCAL_LDLIBS := -llog

include $(BUILD_SHARED_EXECUTABLE)

########################################################
## shadowsocks-libev tunnel
########################################################

include $(CLEAR_VARS)

SHADOWSOCKS_SOURCES := tunnel.c \
	cache.c udprelay.c utils.c netutils.c json.c jconf.c \
	crypto.c aead.c stream.c base64.c \
	plugin.c \
	android.c

LOCAL_MODULE    := ss-tunnel
LOCAL_SRC_FILES := $(addprefix shadowsocks-libev/src/, $(SHADOWSOCKS_SOURCES))
LOCAL_CFLAGS    := -Wall -O2 -fno-strict-aliasing -DMODULE_TUNNEL \
					-DUSE_CRYPTO_MBEDTLS -DANDROID -DHAVE_CONFIG_H -DSSTUNNEL_JNI \
					-DCONNECT_IN_PROGRESS=EINPROGRESS \
					-I$(LOCAL_PATH)/libancillary \
					-I$(LOCAL_PATH)/include \
					-I$(LOCAL_PATH)/libudns \
					-I$(LOCAL_PATH)/shadowsocks-libev/libcork/include \
					-I$(LOCAL_PATH)/libsodium/src/libsodium/include \
					-I$(LOCAL_PATH)/libsodium/src/libsodium/include/sodium \
					-I$(LOCAL_PATH)/mbedtls/include \
					-I$(LOCAL_PATH)/libev \
					-I$(LOCAL_PATH)/include/shadowsocks-libev

LOCAL_STATIC_LIBRARIES := libev libmbedtls libsodium libcork libudns libancillary

LOCAL_LDLIBS := -llog

include $(BUILD_SHARED_EXECUTABLE)

########################################################
## jni-helper
########################################################

include $(CLEAR_VARS)

LOCAL_MODULE:= jni-helper

LOCAL_C_INCLUDES:= $(LOCAL_PATH)/libancillary

LOCAL_SRC_FILES:= jni-helper.cpp

LOCAL_LDLIBS := -ldl -llog

LOCAL_STATIC_LIBRARIES := cpufeatures libancillary

include $(BUILD_SHARED_LIBRARY)

########################################################
## tun2socks
########################################################

include $(CLEAR_VARS)

LOCAL_CFLAGS := -std=gnu99
LOCAL_CFLAGS += -DBADVPN_THREADWORK_USE_PTHREAD -DBADVPN_LINUX -DBADVPN_BREACTOR_BADVPN -D_GNU_SOURCE
LOCAL_CFLAGS += -DBADVPN_USE_SELFPIPE -DBADVPN_USE_EPOLL
LOCAL_CFLAGS += -DBADVPN_LITTLE_ENDIAN -DBADVPN_THREAD_SAFE
LOCAL_CFLAGS += -DNDEBUG -DANDROID
# LOCAL_CFLAGS += -DTUN2SOCKS_JNI

LOCAL_STATIC_LIBRARIES := libancillary

LOCAL_C_INCLUDES:= \
		$(LOCAL_PATH)/libancillary \
        $(LOCAL_PATH)/badvpn/lwip/src/include/ipv4 \
        $(LOCAL_PATH)/badvpn/lwip/src/include/ipv6 \
        $(LOCAL_PATH)/badvpn/lwip/src/include \
        $(LOCAL_PATH)/badvpn/lwip/custom \
        $(LOCAL_PATH)/badvpn/

TUN2SOCKS_SOURCES := \
        base/BLog_syslog.c \
        system/BReactor_badvpn.c \
        system/BSignal.c \
        system/BConnection_common.c \
        system/BConnection_unix.c \
        system/BTime.c \
        system/BUnixSignal.c \
        system/BNetwork.c \
        flow/StreamRecvInterface.c \
        flow/PacketRecvInterface.c \
        flow/PacketPassInterface.c \
        flow/StreamPassInterface.c \
        flow/SinglePacketBuffer.c \
        flow/BufferWriter.c \
        flow/PacketBuffer.c \
        flow/PacketStreamSender.c \
        flow/PacketPassConnector.c \
        flow/PacketProtoFlow.c \
        flow/PacketPassFairQueue.c \
        flow/PacketProtoEncoder.c \
        flow/PacketProtoDecoder.c \
        socksclient/BSocksClient.c \
        tuntap/BTap.c \
        lwip/src/core/timers.c \
        lwip/src/core/udp.c \
        lwip/src/core/memp.c \
        lwip/src/core/init.c \
        lwip/src/core/pbuf.c \
        lwip/src/core/tcp.c \
        lwip/src/core/tcp_out.c \
        lwip/src/core/netif.c \
        lwip/src/core/def.c \
        lwip/src/core/mem.c \
        lwip/src/core/tcp_in.c \
        lwip/src/core/stats.c \
        lwip/src/core/inet_chksum.c \
        lwip/src/core/ipv4/icmp.c \
        lwip/src/core/ipv4/igmp.c \
        lwip/src/core/ipv4/ip4_addr.c \
        lwip/src/core/ipv4/ip_frag.c \
        lwip/src/core/ipv4/ip4.c \
        lwip/src/core/ipv4/autoip.c \
        lwip/src/core/ipv6/ethip6.c \
        lwip/src/core/ipv6/inet6.c \
        lwip/src/core/ipv6/ip6_addr.c \
        lwip/src/core/ipv6/mld6.c \
        lwip/src/core/ipv6/dhcp6.c \
        lwip/src/core/ipv6/icmp6.c \
        lwip/src/core/ipv6/ip6.c \
        lwip/src/core/ipv6/ip6_frag.c \
        lwip/src/core/ipv6/nd6.c \
        lwip/custom/sys.c \
        tun2socks/tun2socks.c \
        base/DebugObject.c \
        base/BLog.c \
        base/BPending.c \
		system/BDatagram_unix.c \
        flowextra/PacketPassInactivityMonitor.c \
        tun2socks/SocksUdpGwClient.c \
        udpgw_client/UdpGwClient.c

LOCAL_MODULE := tun2socks

LOCAL_LDLIBS := -ldl -llog

LOCAL_SRC_FILES := $(addprefix badvpn/, $(TUN2SOCKS_SOURCES))

include $(BUILD_SHARED_EXECUTABLE)

########################################################
## mbed TLS 
########################################################

include $(CLEAR_VARS)

LOCAL_MODULE := mbedtls

LOCAL_C_INCLUDES := $(LOCAL_PATH)/mbedtls/include

MBEDTLS_SOURCES := $(wildcard $(LOCAL_PATH)/mbedtls/library/*.c)

LOCAL_SRC_FILES := $(MBEDTLS_SOURCES:$(LOCAL_PATH)/%=%)

include $(BUILD_STATIC_LIBRARY)

########################################################
## pcre 
########################################################

include $(CLEAR_VARS)

LOCAL_MODULE := pcre

LOCAL_CFLAGS += -DHAVE_CONFIG_H

LOCAL_C_INCLUDES := $(LOCAL_PATH)/pcre/dist $(LOCAL_PATH)/pcre

libpcre_src_files := \
    pcre_chartables.c \
    dist/pcre_byte_order.c \
    dist/pcre_compile.c \
    dist/pcre_config.c \
    dist/pcre_dfa_exec.c \
    dist/pcre_exec.c \
    dist/pcre_fullinfo.c \
    dist/pcre_get.c \
    dist/pcre_globals.c \
    dist/pcre_jit_compile.c \
    dist/pcre_maketables.c \
    dist/pcre_newline.c \
    dist/pcre_ord2utf8.c \
    dist/pcre_refcount.c \
    dist/pcre_string_utils.c \
    dist/pcre_study.c \
    dist/pcre_tables.c \
    dist/pcre_ucd.c \
    dist/pcre_valid_utf8.c \
    dist/pcre_version.c \
    dist/pcre_xclass.c

LOCAL_SRC_FILES := $(addprefix pcre/, $(libpcre_src_files))

include $(BUILD_STATIC_LIBRARY)

# Import cpufeatures
$(call import-module,android/cpufeatures)

