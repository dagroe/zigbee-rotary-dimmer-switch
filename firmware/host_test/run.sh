#!/bin/sh
# Host test of main/switch_driver.c against stub ESP-IDF headers; needs only a C compiler.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/freertos" "$TMP/driver"

cat > "$TMP/esp_log.h" <<'EOF'
#include <stdio.h>
#define ESP_LOGE(t, ...) ((void)t, printf(__VA_ARGS__), puts(""))
#define ESP_LOGI(t, ...) ((void)t)
#define ESP_LOG_INFO 3
#define esp_log_level_set(t, l) ((void)0)
#define esp_err_to_name(e) "err"
#define ESP_ERROR_CHECK(x) ((void)(x))
#define ESP_OK 0
typedef int esp_err_t;
EOF
cat > "$TMP/esp_timer.h" <<'EOF'
#include <stdint.h>
int64_t esp_timer_get_time(void);
EOF
cat > "$TMP/freertos/FreeRTOS.h" <<'EOF'
typedef int BaseType_t;
#define pdPASS 1
#define pdMS_TO_TICKS(x) (x)
EOF
cat > "$TMP/freertos/task.h" <<'EOF'
#define xTaskCreate(fn, name, stack, arg, prio, h) ((void)(fn), pdPASS)
#define vTaskDelay(x) ((void)0)
EOF
cat > "$TMP/driver/gpio.h" <<'EOF'
#include <stdint.h>
#include <stdbool.h>
typedef int gpio_num_t;
typedef struct { int intr_type; uint64_t pin_bit_mask; int mode; int pull_up_en; int pull_down_en; } gpio_config_t;
#define GPIO_INTR_DISABLE 0
#define GPIO_MODE_INPUT 1
#define ESP_INTR_FLAG_IRAM 0
int gpio_get_level(int pin);
#define gpio_config(c) 0
#define gpio_install_isr_service(f) 0
#define gpio_uninstall_isr_service() ((void)0)
EOF
: > "$TMP/esp_zigbee_core.h"

${CC:-cc} -Wall -Wextra -Wno-unused-parameter -Wno-unused-function \
    -I "$TMP" -I "$HERE/../main" "$HERE/test_switch_driver.c" -o "$TMP/test_switch_driver"
"$TMP/test_switch_driver"
