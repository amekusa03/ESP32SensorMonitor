idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_lcd esp_wifi esp_event nvs_flash esp_netif driver esp_timer lwip esp_http_client json mdns)
