/*
 * SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: LicenseRef-Included
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Espressif Systems
 *    integrated circuit in a product or a software update for such product,
 *    must reproduce the above copyright notice, this list of conditions and
 *    the following disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * 4. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "switch_driver.h"
#include "device_config.h"

#define SWITCH_MAX_BUTTONS    8
#define SWITCH_POLL_PERIOD_MS 10

typedef struct {
    switch_state_t state;
    bool last_value;
    uint32_t last_value_time;
    uint32_t down_time;
} button_ctx_t;

static switch_func_pair_t *switch_func_pair = NULL;
static esp_switch_callback_t func_ptr = NULL;
static uint8_t switch_num = 0;
static button_ctx_t button_ctx[SWITCH_MAX_BUTTONS];
static const char *TAG = "ESP_ZB_SWITCH";

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void button_poll(switch_func_pair_t *pair, button_ctx_t *b)
{
    bool value = gpio_get_level((gpio_num_t)pair->pin);
    uint32_t now = now_ms();

    if (value != b->last_value) {
        b->last_value = value;
        b->last_value_time = now;
        return;
    }
    if (now - b->last_value_time < BUTTON_DEBOUNCE_DURATION_MS) {
        return;
    }

    bool pressed = (value == GPIO_INPUT_LEVEL_ON);
    switch_state_t event = SWITCH_IDLE;

    switch (b->state) {
    case SWITCH_IDLE:
        if (pressed) {
            b->down_time = b->last_value_time;
            b->state = event = SWITCH_PRESS_DETECTED;
        }
        break;
    case SWITCH_PRESS_DETECTED:
        if (!pressed) {
            b->state = SWITCH_IDLE;
            event = SWITCH_RELEASE_DETECTED;
        } else if (now - b->down_time >= BUTTON_LONG_PRESS_DURATION_MS) {
            b->state = event = SWITCH_LONG_PRESS_DETECTED;
        }
        break;
    case SWITCH_LONG_PRESS_DETECTED:
        if (!pressed) {
            b->state = SWITCH_IDLE;
            event = SWITCH_LONG_RELEASE_DETECTED;
        } else if (now - b->down_time >= BUTTON_HOLD_DURATION_MS) {
            b->state = event = SWITCH_HOLD_DETECTED;
        }
        break;
    case SWITCH_HOLD_DETECTED:
        if (!pressed) {
            b->state = SWITCH_IDLE;
            event = SWITCH_HOLD_RELEASE_DETECTED;
        }
        break;
    default:
        b->state = SWITCH_IDLE;
        break;
    }

    if (event != SWITCH_IDLE) {
#ifdef DEBUG_ENABLED
        ESP_LOGI(TAG, "GPIO %d - event %d", (int)pair->pin, event);
#endif
        (*func_ptr)(pair, event);
    }
}

static void switch_driver_button_detected(void *arg)
{
    for (;;) {
        for (int i = 0; i < switch_num; ++i) {
            button_poll(switch_func_pair + i, &button_ctx[i]);
        }
        vTaskDelay(pdMS_TO_TICKS(SWITCH_POLL_PERIOD_MS));
    }
}

static bool switch_driver_gpio_init(switch_func_pair_t *button_func_pair, uint8_t button_num)
{
    if (button_num > SWITCH_MAX_BUTTONS) {
        ESP_LOGE(TAG, "Too many buttons (%d > %d)", button_num, SWITCH_MAX_BUTTONS);
        return false;
    }
    gpio_config_t io_conf = {};
    switch_func_pair = button_func_pair;
    switch_num = button_num;
    uint64_t pin_bit_mask = 0;

    for (int i = 0; i < button_num; ++i) {
        pin_bit_mask |= (1ULL << (button_func_pair + i)->pin);
    }
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = pin_bit_mask;
    io_conf.mode = GPIO_MODE_INPUT;
    /* Enable the internal pull-up on every button input. The on-board buttons
       already have external pull-ups (parallel internal pull-up is harmless),
       but the external daughterboard ports (encoder button, wall switch) may
       have nothing wired in: without a pull-up they would float and fire
       phantom presses. With it, an unconnected input idles high = released. */
    io_conf.pull_up_en = 1;
    io_conf.pull_down_en = 0;
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    /* A contact already closed at boot starts as pressed without reporting it. */
    uint32_t now = now_ms();
    for (int i = 0; i < button_num; ++i) {
        button_ctx_t *b = &button_ctx[i];
        b->last_value = gpio_get_level((gpio_num_t)(button_func_pair + i)->pin);
        b->last_value_time = now;
        b->down_time = now;
        b->state = (b->last_value == GPIO_INPUT_LEVEL_ON) ? SWITCH_PRESS_DETECTED : SWITCH_IDLE;
    }

    /* Not used by the buttons, but rotary_encoder.c registers its handlers on it. */
    esp_err_t err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_install_isr_service failed: %s", esp_err_to_name(err));
        return false;
    }

    BaseType_t created = xTaskCreate(switch_driver_button_detected, "button_detected", 4096, NULL, 10, NULL);
    if (created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create button_detected task");
        gpio_uninstall_isr_service();
        return false;
    }
    return true;
}

bool switch_driver_init(switch_func_pair_t *button_func_pair, uint8_t button_num, esp_switch_callback_t cb)
{
    esp_log_level_set(TAG, ESP_LOG_INFO); // ensure INFO visible for this tag

    func_ptr = cb;
    if (!switch_driver_gpio_init(button_func_pair, button_num)) {
        func_ptr = NULL;
        return false;
    }
#ifdef DEBUG_ENABLED
    ESP_LOGI(TAG, "switch_driver_init init success");
#endif
    return true;
}
