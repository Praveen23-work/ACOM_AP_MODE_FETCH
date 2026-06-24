idf_component_register(SRCS "BLE.c"
                    INCLUDE_DIRS "include"
                    REQUIRES nvs_flash bt
                    )