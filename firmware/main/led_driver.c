#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "led_strip.h"
#include "led_driver.h"


static const char *TAG = "ESP_LED_TASK";

/// LED strip common configuration
led_strip_config_t strip_config = {
    .strip_gpio_num = LED_GPIO,  // The GPIO that connected to the LED strip's data line
    .max_leds = 1,                 // The number of LEDs in the strip,
    .led_model = LED_MODEL_WS2812, // LED strip model, it determines the bit timing
    .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB, // The color component format is G-R-B
    .flags = {
        .invert_out = false, // don't invert the output signal
    }
};

/// RMT backend specific configuration
led_strip_rmt_config_t rmt_config = {
    .clk_src = RMT_CLK_SRC_DEFAULT,    // different clock source can lead to different power consumption
    .resolution_hz = 10 * 1000 * 1000, // RMT counter clock frequency: 10MHz
    .mem_block_symbols = 64,           // the memory size of each RMT channel, in words (4 bytes)
    .flags = {
        .with_dma = false, // DMA feature is available on chips like ESP32-S3/P4
    }
};

/// Create the LED strip object
led_strip_handle_t led_strip;

QueueHandle_t led_evt_queue;

// Blink helpers at file scope (C-safe)
static inline void led_configure_blink(uint8_t r, uint8_t g, uint8_t b, bool one_shot,
                                       bool *blink_enabled_p, bool *blink_one_shot_p, bool *blink_on_phase_p,
                                       uint8_t *solid_r_p, uint8_t *solid_g_p, uint8_t *solid_b_p,
                                       TickType_t *phase_deadline_p, TickType_t half_period_ticks) {
    *blink_enabled_p = true;
    *blink_one_shot_p = one_shot;
    *blink_on_phase_p = true; // start with ON
    *solid_r_p = r; *solid_g_p = g; *solid_b_p = b;
    set_led_rgb(*solid_r_p, *solid_g_p, *solid_b_p);
    *phase_deadline_p = xTaskGetTickCount() + half_period_ticks;
}

static inline void led_set_solid_color(uint8_t r, uint8_t g, uint8_t b,
                                       bool *blink_enabled_p, bool *blink_one_shot_p) {
    *blink_enabled_p = false;
    *blink_one_shot_p = false;
    set_led_rgb(r, g, b);
}

// c
static void led_task(void *pvParameters) {
    ESP_LOGI(TAG, "LED task running");

    led_state_t current_state = LED_COLOR_STATE_OFF;
    led_state_t requested_state = LED_COLOR_STATE_OFF;

    const TickType_t tick_50ms = 50 / portTICK_PERIOD_MS;
    const TickType_t blink_period_ticks = 400 / portTICK_PERIOD_MS; // 400ms period
    const TickType_t blink_half_period = blink_period_ticks / 2;

    bool blink_enabled = false;
    bool blink_one_shot = false;
    bool blink_on_phase = false;
    TickType_t phase_deadline = xTaskGetTickCount();

    uint8_t solid_r = 0, solid_g = 0, solid_b = 0;

    // Initialize OFF
    led_set_solid_color(0, 0, 0, &blink_enabled, &blink_one_shot);
    current_state = LED_COLOR_STATE_OFF;

    uint8_t led_brightness = 5;

    while (1) {
        if (xQueueReceive(led_evt_queue, &requested_state, tick_50ms) == pdTRUE) {
            if (requested_state != current_state) {
                ESP_LOGI(TAG, "LED event: code %u", requested_state);
                current_state = requested_state;

                switch (requested_state) {
                    case LED_COLOR_STATE_OFF:
                        led_set_solid_color(0, 0, 0, &blink_enabled, &blink_one_shot);
                        break;
                    case LED_COLOR_STATE_WARN_RED:
                        led_set_solid_color(led_brightness, 0, 0, &blink_enabled, &blink_one_shot);
                        break;
                    case LED_COLOR_STATE_OK_GREEN:
                        led_set_solid_color(0, led_brightness, 0, &blink_enabled, &blink_one_shot);
                        break;

                    case LED_COLOR_STATE_FEEDBACK_WHITE_ONE_PULSE:
                        led_configure_blink(led_brightness, led_brightness, led_brightness, true,
                                            &blink_enabled, &blink_one_shot, &blink_on_phase,
                                            &solid_r, &solid_g, &solid_b,
                                            &phase_deadline, blink_half_period);
                        break;
                    case LED_COLOR_STATE_WATITNG_YELLOW_BLINK:
                        led_configure_blink(led_brightness, led_brightness, 0, false,
                                            &blink_enabled, &blink_one_shot, &blink_on_phase,
                                            &solid_r, &solid_g, &solid_b,
                                            &phase_deadline, blink_half_period);
                        break;

                    // Continuous blink colors
                    case LED_COLOR_STATE_BLINK_RED:
                        led_configure_blink(led_brightness, 0, 0, false,
                                            &blink_enabled, &blink_one_shot, &blink_on_phase,
                                            &solid_r, &solid_g, &solid_b,
                                            &phase_deadline, blink_half_period);
                        break;
                    case LED_COLOR_STATE_BLINK_GREEN:
                        led_configure_blink(0, led_brightness, 0, false,
                                            &blink_enabled, &blink_one_shot, &blink_on_phase,
                                            &solid_r, &solid_g, &solid_b,
                                            &phase_deadline, blink_half_period);
                        break;
                    case LED_COLOR_STATE_BLINK_BLUE:
                        led_configure_blink(0, 0, led_brightness, false,
                                            &blink_enabled, &blink_one_shot, &blink_on_phase,
                                            &solid_r, &solid_g, &solid_b,
                                            &phase_deadline, blink_half_period);
                        break;
                    case LED_COLOR_STATE_BLINK_YELLOW:
                        led_configure_blink(led_brightness, led_brightness, 0, false,
                                            &blink_enabled, &blink_one_shot, &blink_on_phase,
                                            &solid_r, &solid_g, &solid_b,
                                            &phase_deadline, blink_half_period);
                        break;
                    case LED_COLOR_STATE_BLINK_WHITE:
                        led_configure_blink(led_brightness, led_brightness, led_brightness, false,
                                            &blink_enabled, &blink_one_shot, &blink_on_phase,
                                            &solid_r, &solid_g, &solid_b,
                                            &phase_deadline, blink_half_period);
                        break;

                    // One-shot blink colors
                    case LED_COLOR_STATE_BLINK_ONCE_RED:
                        led_configure_blink(led_brightness, 0, 0, true,
                                            &blink_enabled, &blink_one_shot, &blink_on_phase,
                                            &solid_r, &solid_g, &solid_b,
                                            &phase_deadline, blink_half_period);
                        break;
                    case LED_COLOR_STATE_BLINK_ONCE_GREEN:
                        led_configure_blink(0, led_brightness, 0, true,
                                            &blink_enabled, &blink_one_shot, &blink_on_phase,
                                            &solid_r, &solid_g, &solid_b,
                                            &phase_deadline, blink_half_period);
                        break;
                    case LED_COLOR_STATE_BLINK_ONCE_BLUE:
                        led_configure_blink(0, 0, led_brightness, true,
                                            &blink_enabled, &blink_one_shot, &blink_on_phase,
                                            &solid_r, &solid_g, &solid_b,
                                            &phase_deadline, blink_half_period);
                        break;
                    case LED_COLOR_STATE_BLINK_ONCE_YELLOW:
                        led_configure_blink(led_brightness, led_brightness, 0, true,
                                            &blink_enabled, &blink_one_shot, &blink_on_phase,
                                            &solid_r, &solid_g, &solid_b,
                                            &phase_deadline, blink_half_period);
                        break;
                    case LED_COLOR_STATE_BLINK_ONCE_WHITE:
                        led_configure_blink(led_brightness, led_brightness, led_brightness, true,
                                            &blink_enabled, &blink_one_shot, &blink_on_phase,
                                            &solid_r, &solid_g, &solid_b,
                                            &phase_deadline, blink_half_period);
                        break;
                    default:
                        break;
                }
            }
        }

        if (blink_enabled) {
            TickType_t now = xTaskGetTickCount();
            if (now >= phase_deadline) {
                blink_on_phase = !blink_on_phase;
                if (blink_on_phase) {
                    set_led_rgb(solid_r, solid_g, solid_b);
                } else {
                    set_led_rgb(0, 0, 0);
                }

                if (blink_one_shot && !blink_on_phase) {
                    blink_enabled = false;
                    blink_one_shot = false;
                    current_state = LED_COLOR_STATE_OFF;
                }

                phase_deadline = now + blink_half_period;
            }
        }
    }
}



static void init_led_task() {
    // create light strip
    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "led_strip_new_rmt_device failed: %s", esp_err_to_name(err));
        return; // or handle error
    }
    xTaskCreate(led_task, "LED_main", 4096, NULL, 6, NULL);

}

void set_led_rgb(uint8_t r, uint8_t g, uint8_t b) {
    esp_err_t err;
    if (!led_strip) {
        ESP_LOGE(TAG, "led_strip handle is NULL");
        return;
    }
    err = led_strip_set_pixel(led_strip, 0, r, g, b);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "led_strip_set_pixel failed: %s", esp_err_to_name(err));
        return;
    }
    err = led_strip_refresh(led_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "led_strip_refresh failed: %s", esp_err_to_name(err));
        return;
    }
}

void setup_led_strip(QueueHandle_t led_evt_queue_p) {
    led_evt_queue = led_evt_queue_p;
    init_led_task();
}