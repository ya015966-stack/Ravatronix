idf_component_register(SRCS "modem_at_parser.c" "modem_at_service.c"
                       INCLUDE_DIRS "include"
                       REQUIRES driver board_config)
