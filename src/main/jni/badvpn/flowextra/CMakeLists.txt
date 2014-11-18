add_library(flowextra
    PacketPassInactivityMonitor.c
    KeepaliveIO.c
)
target_link_libraries(flowextra flow system)
