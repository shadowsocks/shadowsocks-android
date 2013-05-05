add_library(server_conection
    ServerConnection.c
    SCKeepaliveSource.c
)
target_link_libraries(server_conection system flow flowextra nspr_support ${NSPR_LIBRARIES} ${NSS_LIBRARIES})
