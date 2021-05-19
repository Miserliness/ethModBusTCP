set(srcs "tcp_slave.c"
         "enc28j60ethernet.c"
         "modbus_params.c"
         "esp_eth_mac_enc28j60.c"
         "esp_eth_phy_enc28j60.c")

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS ".")
