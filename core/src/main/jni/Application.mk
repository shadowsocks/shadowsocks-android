APP_CFLAGS := -fdata-sections -ffunction-sections -fvisibility=hidden -fvisibility-inlines-hidden
APP_LDFLAGS := -Wl,--hash-style=both -Wl,-exclude-libs,ALL -Wl,--gc-sections
APP_THIN_ARCHIVE := true
APP_STL := c++_static
