idf_component_register(SRCS "logs_handler.c"
                    INCLUDE_DIRS "include"
                    REQUIRES nvs_flash spiffs GSM_handler)
# target_link_libraries(${COMPONENT_LIB} PRIVATE spi_flash spiffs)