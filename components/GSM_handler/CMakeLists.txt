idf_component_register(SRCS "GSM_handler.c"
                    INCLUDE_DIRS "include"
                    REQUIRES UART_HANDLER logs_handler wifi_handler)
