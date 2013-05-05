add_library(dhcpclientcore
    BDHCPClientCore.c
)
target_link_libraries(dhcpclientcore system flow flowextra badvpn_random)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_library(dhcpclient
        BDHCPClient.c
        DHCPIpUdpEncoder.c
        DHCPIpUdpDecoder.c
    )
    target_link_libraries(dhcpclient system flow dhcpclientcore)
endif ()
