idf_component_register(SRCS "ota_handler.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver app_update nvs_flash esp_timer sensor_config GSM_handler UART_HANDLER support)
