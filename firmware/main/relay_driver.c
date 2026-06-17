#include "relay_driver.h"

#include "driver/gpio.h"
#include "esp_log.h"

#include "device_config.h"

static const char *TAG = "RELAY";
static bool s_relay_on;

void relay_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << RELAY_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    relay_set(false);   // default OFF (de-energized) -- safe state for 230V load
}

void relay_set(bool on)
{
    s_relay_on = on;
    gpio_set_level(RELAY_GPIO, on ? 1 : 0);
    ESP_LOGI(TAG, "Relay %s", on ? "ON" : "OFF");
}

bool relay_get(void)
{
    return s_relay_on;
}
