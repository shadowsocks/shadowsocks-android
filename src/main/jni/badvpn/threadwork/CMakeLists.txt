set(BADVPN_THREADWORK_EXTRA_LIBS)
if (BADVPN_THREADWORK_USE_PTHREAD)
    list(APPEND BADVPN_THREADWORK_EXTRA_LIBS pthread)
endif ()

add_library(threadwork BThreadWork.c)
target_link_libraries(threadwork system ${BADVPN_THREADWORK_EXTRA_LIBS})
