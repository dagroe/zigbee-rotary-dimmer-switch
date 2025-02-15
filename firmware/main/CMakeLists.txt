idf_component_register(
    SRCS
    "esp_zb_switch.c"
    "switch_driver.c"
    "rotary_encoder.c"
    REQUIRES log driver nvs_flash esp_timer
    INCLUDE_DIRS "."
)
