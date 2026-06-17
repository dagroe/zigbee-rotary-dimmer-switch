idf_component_register(
    SRCS
    "device_config.c"
    "esp_zb_switch.c"
    "switch_driver.c"
    "led_driver.c"
    "rotary_encoder.c"
    "ota_driver.c"
    "relay_driver.c"
    REQUIRES log driver nvs_flash esp_timer app_update esp_system esp_partition
    INCLUDE_DIRS "."
)
