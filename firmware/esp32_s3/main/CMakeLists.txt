idf_component_register(SRCS "app_main.c"
                       INCLUDE_DIRS "."
                       REQUIRES board_config camera_service audio_service wifi_service modem_at_service esp_system esp_psram freertos)
