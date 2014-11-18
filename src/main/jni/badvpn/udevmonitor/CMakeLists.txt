add_library(udevmonitor
    NCDUdevMonitorParser.c
    NCDUdevMonitor.c
    NCDUdevCache.c
    NCDUdevManager.c
)
target_link_libraries(udevmonitor system flow stringmap)
