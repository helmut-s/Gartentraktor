idf_component_register(SRCS "main.cpp" "wifi_service.cpp" "ota_server.cpp" "control_logic.cpp" "vesc_can.cpp"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES espressif__esp_twai_mcp2515 esp_driver_twai esp_driver_gpio esp_driver_spi
                                  nvs_flash esp_wifi esp_event esp_netif
                                  esp_http_server app_update esp_partition)
