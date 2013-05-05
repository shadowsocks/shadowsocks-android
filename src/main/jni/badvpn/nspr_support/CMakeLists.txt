add_library(nspr_support
    DummyPRFileDesc.c
    BSSLConnection.c
)
target_link_libraries(nspr_support system flow threadwork ${NSPR_LIBRARIES} ${NSS_LIBRARIES})
