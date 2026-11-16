idf_component_register(SRCS "network_service.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_modem esp_netif esp_event board_config)