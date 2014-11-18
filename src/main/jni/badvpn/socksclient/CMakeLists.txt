add_library(socksclient
    BSocksClient.c
)
target_link_libraries(socksclient system flow flowextra)
