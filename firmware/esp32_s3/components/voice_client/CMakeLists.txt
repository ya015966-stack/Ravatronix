idf_component_register(SRCS "voice_client.c" "wav_codec.c"
                       INCLUDE_DIRS "include"
                       REQUIRES audio_service esp_http_client mbedtls esp_timer)
