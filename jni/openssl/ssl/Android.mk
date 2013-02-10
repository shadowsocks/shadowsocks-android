LOCAL_PATH:= $(call my-dir)

local_c_includes := \
	$(NDK_PROJECT_PATH)/jni/openssl \
	$(NDK_PROJECT_PATH)/jni/openssl/include \
	$(NDK_PROJECT_PATH)/jni/openssl/crypto \
	$(NDK_PROJECT_PATH)/jni/openss/include/openssl
	
local_src_files:= \
	s2_meth.c \
	s2_srvr.c \
	s2_clnt.c \
	s2_lib.c \
	s2_enc.c \
	s2_pkt.c \
	s3_meth.c \
	s3_srvr.c \
	s3_clnt.c \
	s3_lib.c \
	s3_enc.c \
	s3_pkt.c \
	s3_both.c \
	s23_meth.c \
	s23_srvr.c \
	s23_clnt.c \
	s23_lib.c \
	s23_pkt.c \
	t1_meth.c \
	t1_srvr.c \
	t1_clnt.c \
	t1_lib.c \
	t1_enc.c \
	t1_reneg.c \
	d1_meth.c \
	d1_srvr.c \
	d1_clnt.c \
	d1_lib.c \
	d1_pkt.c \
	d1_both.c \
	d1_enc.c \
	d1_srtp.c \
	ssl_lib.c \
	ssl_err2.c \
	ssl_cert.c \
	ssl_sess.c \
	ssl_ciph.c \
	ssl_stat.c \
	ssl_rsa.c \
	ssl_asn1.c \
	ssl_txt.c \
	ssl_algs.c \
	bio_ssl.c \
	ssl_err.c \
	kssl.c \
	tls_srp.c

include $(CLEAR_VARS)
include $(LOCAL_PATH)/../android-config.mk
LOCAL_SRC_FILES += $(local_src_files)
LOCAL_C_INCLUDES += $(local_c_includes)
LOCAL_SHARED_LIBRARIES += libcrypto
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:= libssl
include $(BUILD_SHARED_LIBRARY)

ifeq ($(WITH_HOST_DALVIK),true)
    include $(CLEAR_VARS)
    include $(LOCAL_PATH)/../android-config.mk
    LOCAL_SRC_FILES += $(local_src_files)
    LOCAL_C_INCLUDES += $(local_c_includes)
    LOCAL_SHARED_LIBRARIES += libcrypto
    LOCAL_MODULE_TAGS := optional
    LOCAL_MODULE:= libssl
    include $(BUILD_SHARED_LIBRARY)
endif

# ssltest
include $(CLEAR_VARS)
include $(LOCAL_PATH)/../android-config.mk
LOCAL_SRC_FILES:= ssltest.c
LOCAL_C_INCLUDES += $(local_c_includes)
LOCAL_SHARED_LIBRARIES := libssl libcrypto
LOCAL_MODULE:= ssltest
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)
