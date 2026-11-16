idf_component_register(SRCS "backend_client.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_http_client mbedtls camera_service)