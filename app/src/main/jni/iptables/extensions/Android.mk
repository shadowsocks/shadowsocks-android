LOCAL_PATH:= $(call my-dir)
#----------------------------------------------------------------
## extension

MY_srcdir:=$(LOCAL_PATH)
# Exclude some modules that are problematic to compile (types/header).
MY_excluded_modules:=TCPOPTSTRIP

MY_pfx_build_mod := $(patsubst ${MY_srcdir}/libxt_%.c,%,$(wildcard ${MY_srcdir}/libxt_*.c))
MY_pf4_build_mod := $(patsubst ${MY_srcdir}/libipt_%.c,%,$(wildcard ${MY_srcdir}/libipt_*.c))
MY_pf6_build_mod := $(patsubst ${MY_srcdir}/libip6t_%.c,%,$(wildcard ${MY_srcdir}/libip6t_*.c))
MY_pfx_build_mod := $(filter-out ${MY_excluded_modules} dccp ipvs,${MY_pfx_build_mod})
MY_pf4_build_mod := $(filter-out ${MY_excluded_modules} dccp ipvs,${MY_pf4_build_mod})
MY_pf6_build_mod := $(filter-out ${MY_excluded_modules} dccp ipvs,${MY_pf6_build_mod})
MY_pfx_objs      := $(patsubst %,libxt_%.o,${MY_pfx_build_mod})
MY_pf4_objs      := $(patsubst %,libipt_%.o,${MY_pf4_build_mod})
MY_pf6_objs      := $(patsubst %,libip6t_%.o,${MY_pf6_build_mod})

#----------------------------------------------------------------
# libext
# TODO(jpa): Trun this into a function/macro as libext{,4,6} are all the same.

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS:=
LOCAL_MODULE:=libext

# LOCAL_MODULE_CLASS must be defined before calling $(local-intermediates-dir)
#
LOCAL_MODULE_CLASS := STATIC_LIBRARIES

# LOCAL_PATH needed because of dirty #include "blabla.c"
LOCAL_C_INCLUDES:= \
	$(LOCAL_PATH)/../include/ \
	$(KERNEL_HEADERS) \
	$(LOCAL_PATH)

LOCAL_CFLAGS:=-DNO_SHARED_LIBS=1
# The $* does not work as expected. It ends up empty. Even with SECONDEXPANSION.
# LOCAL_CFLAGS+=-D_INIT=lib$*_init
LOCAL_CFLAGS+=-DXTABLES_INTERNAL
# Accommodate arm-eabi-4.4.3 tools that don't set __ANDROID__
LOCAL_CFLAGS+=-D__ANDROID__

MY_initext_func := $(addprefix xt_,${MY_pfx_build_mod})
MY_GEN_INITEXT:= $(LOCAL_PATH)/gen_initext.c
$(info $(shell \
	if [ ! -e $(MY_GEN_INITEXT) ]; then \
	echo "GEN initext.c"; \
	echo "" >$(MY_GEN_INITEXT); \
	for i in $(MY_initext_func); do \
		echo "extern void lib$${i}_init(void);" >>$(MY_GEN_INITEXT); \
	done; \
	echo "void init_extensions(void);" >>$(MY_GEN_INITEXT); \
	echo "void init_extensions(void)" >>$(MY_GEN_INITEXT); \
	echo "{" >>$(MY_GEN_INITEXT); \
	for i in $(MY_initext_func); do \
		echo " ""lib$${i}_init();" >>$(MY_GEN_INITEXT); \
		sed "s/_init(void)/lib$${i}_init(void)/" $(LOCAL_PATH)/lib$${i}.c > $(LOCAL_PATH)/gen_lib$${i}.c; \
	done; \
	echo "}" >>$(MY_GEN_INITEXT); \
	fi; ))

MY_lib_sources:= \
	$(patsubst %,gen_libxt_%.c,${MY_pfx_build_mod})

LOCAL_SRC_FILES := \
	gen_initext.c \
	$(MY_lib_sources)

include $(BUILD_STATIC_LIBRARY)

#----------------------------------------------------------------
# libext4

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS:=
LOCAL_MODULE:=libext4

# LOCAL_MODULE_CLASS must be defined before calling $(local-intermediates-dir)
#
LOCAL_MODULE_CLASS := STATIC_LIBRARIES

# LOCAL_PATH needed because of dirty #include "blabla.c"
LOCAL_C_INCLUDES:= \
	$(LOCAL_PATH)/../include/ \
	$(KERNEL_HEADERS) \
	$(LOCAL_PATH)/

LOCAL_CFLAGS:=-DNO_SHARED_LIBS=1
# The $* does not work as expected. It ends up empty. Even with SECONDEXPANSION.
# LOCAL_CFLAGS+=-D_INIT=lib$*_init
LOCAL_CFLAGS+=-DXTABLES_INTERNAL
# Accommodate arm-eabi-4.4.3 tools that don't set __ANDROID__
LOCAL_CFLAGS+=-D__ANDROID__

MY_initext4_func  := $(addprefix ipt_,${MY_pf4_build_mod})
MY_GEN_INITEXT4:= $(LOCAL_PATH)/gen_initext4.c
$(info $(shell \
	if [ ! -e $(MY_GEN_INITEXT4) ]; then \
	echo "GEN initext4.c"; \
	echo "" >$(MY_GEN_INITEXT4); \
	for i in $(MY_initext4_func); do \
		echo "extern void lib$${i}_init(void);" >>$(MY_GEN_INITEXT4); \
	done; \
	echo "void init_extensions4(void);" >>$(MY_GEN_INITEXT4); \
	echo "void init_extensions4(void)" >>$(MY_GEN_INITEXT4); \
	echo "{" >>$(MY_GEN_INITEXT4); \
	for i in $(MY_initext4_func); do \
		echo " ""lib$${i}_init();" >>$(MY_GEN_INITEXT4); \
		sed "s/_init(void)/lib$${i}_init(void)/" $(LOCAL_PATH)/lib$${i}.c > $(LOCAL_PATH)/gen_lib$${i}.c; \
	done; \
	echo "}" >>$(MY_GEN_INITEXT4); \
	fi; ))

MY_lib_sources:= \
	$(patsubst %,gen_libipt_%.c,${MY_pf4_build_mod})

LOCAL_SRC_FILES := \
	gen_initext4.c \
	$(MY_lib_sources)

include $(BUILD_STATIC_LIBRARY)

#----------------------------------------------------------------
# libext6

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS:=
LOCAL_MODULE:=libext6

# LOCAL_MODULE_CLASS must be defined before calling $(local-intermediates-dir)
#
LOCAL_MODULE_CLASS := STATIC_LIBRARIES

# LOCAL_PATH needed because of dirty #include "blabla.c"
LOCAL_C_INCLUDES:= \
	$(LOCAL_PATH)/../include/ \
	$(KERNEL_HEADERS) \
	$(LOCAL_PATH)

LOCAL_CFLAGS:=-DNO_SHARED_LIBS=1
# The $* does not work as expected. It ends up empty. Even with SECONDEXPANSION.
# LOCAL_CFLAGS+=-D_INIT=lib$*_init
LOCAL_CFLAGS+=-DXTABLES_INTERNAL
# Accommodate arm-eabi-4.4.3 tools that don't set __ANDROID__
LOCAL_CFLAGS+=-D__ANDROID__

MY_initext6_func := $(addprefix ip6t_,${MY_pf6_build_mod})
MY_GEN_INITEXT6:= $(LOCAL_PATH)/gen_initext6.c
$(info $(shell \
	if [ ! -e $(MY_GEN_INITEXT6) ]; then \
	echo "GEN initext6.c"; \
	echo "" >$(MY_GEN_INITEXT6); \
	for i in $(MY_initext6_func); do \
		echo "extern void lib$${i}_init(void);" >>$(MY_GEN_INITEXT6); \
	done; \
	echo "void init_extensions6(void);" >>$(MY_GEN_INITEXT6); \
	echo "void init_extensions6(void)" >>$(MY_GEN_INITEXT6); \
	echo "{" >>$(MY_GEN_INITEXT6); \
	for i in $(MY_initext6_func); do \
		echo " ""lib$${i}_init();" >>$(MY_GEN_INITEXT6); \
		sed "s/_init(void)/lib$${i}_init(void)/" $(LOCAL_PATH)/lib$${i}.c > $(LOCAL_PATH)/gen_lib$${i}.c; \
	done; \
	echo "}" >>$(MY_GEN_INITEXT6); \
	fi; ))

MY_lib_sources:= \
	$(patsubst %,gen_libip6t_%.c,${MY_pf6_build_mod})

LOCAL_SRC_FILES := \
	gen_initext6.c \
	$(MY_lib_sources)

include $(BUILD_STATIC_LIBRARY)

#----------------------------------------------------------------
