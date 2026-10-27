#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "ext_switch.h"

/* The input is active-low: contact to GND, internal pull-up. */
#define EXT_SWITCH_LEVEL_CLOSED  0
/* The line must be quiet this long before its level is trusted. */
#define EXT_SWITCH_DEBOUNCE_MS   50

static const char *TAG = "EXT_SWITCH";

static gpio_num_t s_pin = GPIO_NUM_NC;
static int s_mode = EXT_SWITCH_MODE_PUSHBUTTON;
static ext_switch_callback_t s_cb = NULL;
static TaskHandle_t s_task = NULL;

static void IRAM_ATTR ext_switch_isr(void *arg)
{
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(s_task, &woken);
    portYIELD_FROM_ISR(woken);
}

/* Edges only wake this task; it never polls. That keeps a rocker that sits
   closed for hours from tying up anything (unlike the shared button task,
   which loops until release). */
static void ext_switch_task(void *arg)
{
    int stable = gpio_get_level(s_pin);
    ESP_LOGI(TAG, "GPIO %d, %s mode, initial level %d", s_pin,
             s_mode == EXT_SWITCH_MODE_ROCKER ? "rocker" : "push-button", stable);
    gpio_intr_enable(s_pin);

    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        /* Every further edge restarts the quiet period, so contact bounce and
           noise spikes collapse into a single read once the line settles. */
        while (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(EXT_SWITCH_DEBOUNCE_MS)) > 0) {
        }

        int level = gpio_get_level(s_pin);
        if (level == stable) {
            continue;   /* glitch: ended where it started */
        }
        stable = level;

        if (s_mode == EXT_SWITCH_MODE_ROCKER || level == EXT_SWITCH_LEVEL_CLOSED) {
            s_cb();
        }
    }
}

bool ext_switch_init(gpio_num_t pin, int mode, ext_switch_callback_t cb)
{
    s_pin = pin;
    s_mode = mode;
    s_cb = cb;

    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_INPUT,
        /* Unconnected port idles high = open, and never fires. */
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed: %s", esp_err_to_name(err));
        return false;
    }
    /* Keep the interrupt off until the task has sampled the initial level. */
    gpio_intr_disable(pin);

    if (xTaskCreate(ext_switch_task, "ext_switch", 3072, NULL, 10, &s_task) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create ext_switch task");
        return false;
    }
    err = gpio_isr_handler_add(pin, ext_switch_isr, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_isr_handler_add failed: %s", esp_err_to_name(err));
        vTaskDelete(s_task);
        s_task = NULL;
        return false;
    }
    return true;
}
