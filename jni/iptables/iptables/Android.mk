LOCAL_PATH:= $(call my-dir)
My_intermediaries := $(call local-intermediates-dir)
#----------------------------------------------------------------
# libxtables

include $(CLEAR_VARS)

LOCAL_C_INCLUDES:= \
	$(LOCAL_PATH)/../include/ \
	$(KERNEL_HEADERS) \

LOCAL_CFLAGS:=-DNO_SHARED_LIBS=1
LOCAL_CFLAGS+=-DXTABLES_INTERNAL
LOCAL_CFLAGS+=-DXTABLES_LIBDIR=\"xtables_libdir_not_used\"
# Accommodate arm-eabi-4.4.3 tools that don't set __ANDROID__
LOCAL_CFLAGS+=-D__ANDROID__

LOCAL_LDFLAGS:=-version-info 6:0:0
LOCAL_SRC_FILES:= \
	xtables.c xtoptions.c

LOCAL_MODULE:=libxtables

include $(BUILD_STATIC_LIBRARY)

#----------------------------------------------------------------
# iptables


include $(CLEAR_VARS)

LOCAL_C_INCLUDES:= \
	$(LOCAL_PATH)/../include/

LOCAL_CFLAGS:=-DNO_SHARED_LIBS=1
LOCAL_CFLAGS+=-DALL_INCLUSIVE
LOCAL_CFLAGS+=-DXTABLES_INTERNAL
# Accommodate arm-eabi-4.4.3 tools that don't set __ANDROID__
LOCAL_CFLAGS+=-D__ANDROID__

LOCAL_SRC_FILES:= \
	iptables-standalone.c iptables.c xshared.c

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:=iptables

LOCAL_STATIC_LIBRARIES := \
	libext \
	libext4 \
	libip4tc \
	libxtables

include $(BUILD_EXECUTABLE)

#----------------------------------------------------------------
# ip6tables
include $(CLEAR_VARS)

LOCAL_C_INCLUDES:= \
	$(LOCAL_PATH)/../include/

LOCAL_CFLAGS:=-DNO_SHARED_LIBS=1
LOCAL_CFLAGS+=-DALL_INCLUSIVE
LOCAL_CFLAGS+=-DXTABLES_INTERNAL
# Accommodate arm-eabi-4.4.3 tools that don't set __ANDROID__
LOCAL_CFLAGS+=-D__ANDROID__

LOCAL_SRC_FILES:= \
	ip6tables-standalone.c ip6tables.c xshared.c

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:=ip6tables

LOCAL_STATIC_LIBRARIES := \
	libext \
	libext6 \
	libip6tc \
	libxtables

include $(BUILD_EXECUTABLE)


#----------------------------------------------------------------
