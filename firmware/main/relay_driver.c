#include "relay_driver.h"

#include "driver/gpio.h"
#include "esp_log.h"

#include "device_config.h"

static const char *TAG = "RELAY";
static bool s_outlet_on;

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
    relay_set(true);   // default: outlet POWERED (matches the de-energized NC default)
}

void relay_set(bool outlet_on)
{
    s_outlet_on = outlet_on;
    // NC wiring: powered = coil de-energized = GPIO low; cut = energized = GPIO high.
    gpio_set_level(RELAY_GPIO, outlet_on ? 0 : 1);
    ESP_LOGI(TAG, "Outlet %s", outlet_on ? "POWERED" : "CUT");
}

bool relay_get(void)
{
    return s_outlet_on;
}
