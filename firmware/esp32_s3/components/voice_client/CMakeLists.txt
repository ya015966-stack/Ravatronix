idf_component_register(SRCS "voice_client.c" "wav_codec.c"
                       INCLUDE_DIRS "include"
                       REQUIRES audio_service network_transport esp_timer)
