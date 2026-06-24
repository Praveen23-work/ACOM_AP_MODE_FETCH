idf_component_register(SRCS "wifi_handler.c"
                    INCLUDE_DIRS "include"
                    REQUIRES 
                    esp_wifi
                    esp_netif
                    esp_event
                    nvs_flash
                    lwip
                    esp_http_client
                    esp_https_ota
                    mbedtls
                    esp_system
                    freertos 
                    app_update
                    json
                    support 
                    sensor_config
                    RTC_handler
                    )
