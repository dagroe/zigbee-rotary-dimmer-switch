#include "relay_driver.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs.h"

#include "device_config.h"

static const char *TAG = "RELAY";

#define RELAY_NVS_NS       "relay"
#define RELAY_NVS_STARTUP  "startup"   // StartUpOnOff value
#define RELAY_NVS_LAST     "last"      // last outlet state (for "previous")

// ZCL StartUpOnOff values
#define RELAY_STARTUP_OFF       0x00
#define RELAY_STARTUP_ON        0x01
#define RELAY_STARTUP_TOGGLE    0x02
#define RELAY_STARTUP_PREVIOUS  0xFF

static bool s_outlet_on;
static uint8_t s_startup = RELAY_STARTUP_PREVIOUS;

static uint8_t nvs_get_or(const char *key, uint8_t def)
{
    nvs_handle_t h;
    uint8_t v = def;
    if (nvs_open(RELAY_NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        nvs_get_u8(h, key, &v);   // leaves v == def if the key is missing
        nvs_close(h);
    }
    return v;
}

static void nvs_put(const char *key, uint8_t val)
{
    nvs_handle_t h;
    if (nvs_open(RELAY_NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u8(h, key, val);
        nvs_commit(h);
        nvs_close(h);
    } else {
        ESP_LOGW(TAG, "nvs_open failed persisting %s", key);
    }
}

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
    relay_set(true);   // safe default: outlet POWERED (NC default), before NVS is up
}

void relay_apply_startup(void)
{
    s_startup = nvs_get_or(RELAY_NVS_STARTUP, RELAY_STARTUP_PREVIOUS);
    bool last = nvs_get_or(RELAY_NVS_LAST, 1) != 0;   // default powered

    bool desired;
    switch (s_startup) {
    case RELAY_STARTUP_OFF:    desired = false;  break;
    case RELAY_STARTUP_ON:     desired = true;   break;
    case RELAY_STARTUP_TOGGLE: desired = !last;  break;
    default:                   desired = last;   break;   // 0xFF = previous
    }
    relay_set(desired);
    relay_remember();   // the applied state is now the "last" state
    ESP_LOGI(TAG, "Power-on behavior 0x%02x -> outlet %s", s_startup, desired ? "POWERED" : "CUT");
}

void relay_set(bool outlet_on)
{
    s_outlet_on = outlet_on;
    // NC wiring: powered = coil de-energized = GPIO low; cut = energized = GPIO high.
    gpio_set_level(RELAY_GPIO, outlet_on ? 0 : 1);
    ESP_LOGI(TAG, "Outlet %s", outlet_on ? "POWERED" : "CUT");
}

void relay_remember(void)
{
    nvs_put(RELAY_NVS_LAST, s_outlet_on ? 1 : 0);
}

bool relay_get(void)
{
    return s_outlet_on;
}

uint8_t relay_get_startup_behavior(void)
{
    return s_startup;
}

void relay_set_startup_behavior(uint8_t mode)
{
    s_startup = mode;
    nvs_put(RELAY_NVS_STARTUP, mode);
    ESP_LOGI(TAG, "Power-on behavior set to 0x%02x", mode);
}
