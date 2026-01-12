idf_component_register(
    SRCS
    "device_config.c"
    "esp_zb_switch.c"
    "switch_driver.c"
    "led_driver.c"
    "rotary_encoder.c"
    REQUIRES log driver nvs_flash esp_timer
    INCLUDE_DIRS "."
)
