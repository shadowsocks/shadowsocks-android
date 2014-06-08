LOCAL_PATH:= $(call my-dir)

#----------------------------------------------------------------
# libip4tc

include $(CLEAR_VARS)

LOCAL_C_INCLUDES:= \
	$(KERNEL_HEADERS) \
	$(LOCAL_PATH)/../include/

# Accommodate arm-eabi-4.4.3 tools that don't set __ANDROID__
LOCAL_CFLAGS:=-D__ANDROID__

LOCAL_SRC_FILES:= \
	libip4tc.c \


LOCAL_MODULE_TAGS:=
LOCAL_MODULE:=libip4tc

include $(BUILD_STATIC_LIBRARY)


#----------------------------------------------------------------
# libip6tc

include $(CLEAR_VARS)

LOCAL_C_INCLUDES:= \
	$(KERNEL_HEADERS) \
	$(LOCAL_PATH)/../include/

# Accommodate arm-eabi-4.4.3 tools that don't set __ANDROID__
LOCAL_CFLAGS:=-D__ANDROID__

LOCAL_SRC_FILES:= \
	libip6tc.c \


LOCAL_MODULE_TAGS:=
LOCAL_MODULE:=libip6tc

include $(BUILD_STATIC_LIBRARY)

#----------------------------------------------------------------
