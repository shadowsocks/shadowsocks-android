add_executable(badvpn-tunctl tunctl.c)

install(
    TARGETS badvpn-tunctl
    RUNTIME DESTINATION bin
)
