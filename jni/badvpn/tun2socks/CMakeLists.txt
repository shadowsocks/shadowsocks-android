add_executable(badvpn-tun2socks
    tun2socks.c
    SocksUdpGwClient.c
)
target_link_libraries(badvpn-tun2socks system flow tuntap lwip socksclient udpgw_client)

install(
    TARGETS badvpn-tun2socks
    RUNTIME DESTINATION bin
)

install(
    FILES badvpn-tun2socks.8
    DESTINATION share/man/man8
)
