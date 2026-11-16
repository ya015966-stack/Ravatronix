idf_component_register(SRCS "app_main.c"
                       INCLUDE_DIRS "."
                       REQUIRES board_config camera_service audio_service network_service backend_client esp_system esp_psram freertos)
