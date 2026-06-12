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
#include "esp_intr_alloc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "switch_driver.h"
#include "device_config.h"

/**
 * @brief:
 * This example code shows how to configure light switch with attribute as well as button switch handler.
 *
 * @note:
   Currently only support toggle switch functionality available
 *
 * @note:
 * For other possible switch functions (on/off,level up/down,step up/down). User need to implement and create them by themselves
 */

static QueueHandle_t gpio_evt_queue = NULL;
/* button function pair, should be defined in switch example source file */
static switch_func_pair_t *switch_func_pair = NULL;
/* call back function pointer */
static esp_switch_callback_t func_ptr = NULL;
/* which button is pressed */
static uint8_t switch_num = 0;
static const char *TAG = "ESP_ZB_SWITCH";

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    if (gpio_evt_queue != NULL) {
        xQueueSendFromISR(gpio_evt_queue, (switch_func_pair_t *)arg, NULL);
    }
}

/**
 * @brief Tasks for checking the button event and debounce the switch state
 *
 * @param arg      Unused value.
 */
static void switch_driver_button_detected(void *arg)
{
    gpio_num_t io_num = GPIO_NUM_NC;
    switch_func_pair_t button_func_pair;
    static switch_state_t switch_state = SWITCH_IDLE;
    bool evt_flag = false;
    uint32_t last_value_time = 0;
    bool last_value = false;

    uint32_t button_down_time = 0;
#ifdef DEBUG_ENABLED
    ESP_LOGI(TAG, "switch_driver_button_detected task start");
#endif
    for (;;) {
#ifdef DEBUG_ENABLED
        ESP_LOGI(TAG, "button task alive"); // use DEBUG to avoid spam
#endif
        /* check if there is any queue received, if yes read out the button_func_pair */
        if (xQueueReceive(gpio_evt_queue, &button_func_pair, portMAX_DELAY)) {
#ifdef DEBUG_ENABLED
            ESP_LOGI(TAG, "switch_driver_button_detected");
#endif
            io_num =  button_func_pair.pin;
            /* Mask only this button while we debounce it, so the other button
               stays responsive (e.g. toggle still works during a 5s commission
               hold). */
            gpio_intr_disable(io_num);
            evt_flag = true;
            // Initialize debounce baseline from current level to avoid false transitions
            // last_value = gpio_get_level(io_num);
            last_value_time = esp_timer_get_time() / 1000;
            // ESP_LOGI(TAG, "GPIO %d - initial level: %d", button_func_pair.pin, last_value);
            // Reset state on new event to ensure clean sequence
              switch_state = SWITCH_IDLE;
        }
        while (evt_flag) {
            bool value = gpio_get_level(io_num);
#ifdef DEBUG_ENABLED
            ESP_LOGI(TAG, "GPIO %d - level: %d", button_func_pair.pin, value);
#endif
            uint32_t value_time = esp_timer_get_time() / 1000;
            // debounce the value first
            if (value != last_value) {
                last_value_time = value_time;
                last_value = value;
#ifdef DEBUG_ENABLED
                ESP_LOGI(TAG, "value !== last_value");
#endif
                vTaskDelay(10 / portTICK_PERIOD_MS);
                continue;
            }

            if (value_time < last_value_time + BUTTON_DEBOUNCE_DURATION_MS) {
#ifdef DEBUG_ENABLED
                ESP_LOGI(TAG, "< debounce");
#endif
                vTaskDelay(10 / portTICK_PERIOD_MS);
                continue;
            }

            // adapt state based on state and value
            switch (switch_state) {
            case SWITCH_IDLE:
                if (value == GPIO_INPUT_LEVEL_ON) {
                    switch_state = SWITCH_PRESS_DETECTED;
                    button_down_time = last_value_time;
#ifdef DEBUG_ENABLED
                    ESP_LOGI(TAG, "SWITCH_PRESS_DETECTED");
#endif
                    /* callback to button_handler */
                    (*func_ptr)(&button_func_pair, SWITCH_PRESS_DETECTED);
                } else {
                    switch_state = SWITCH_IDLE;
                }
                break;
            case SWITCH_PRESS_DETECTED:
                if (value == GPIO_INPUT_LEVEL_ON) {
                    if (value_time >= button_down_time + BUTTON_LONG_PRESS_DURATION_MS) {
                        switch_state = SWITCH_LONG_PRESS_DETECTED;
                        /* callback to button_handler */
                        (*func_ptr)(&button_func_pair, SWITCH_LONG_PRESS_DETECTED);
#ifdef DEBUG_ENABLED
                        ESP_LOGI(TAG, "SWITCH_LONG_PRESS_DETECTED");
#endif
                    }
                } else {
                    switch_state = SWITCH_IDLE;
                    /* callback to button_handler */
                    (*func_ptr)(&button_func_pair, SWITCH_RELEASE_DETECTED);
#ifdef DEBUG_ENABLED
                    ESP_LOGI(TAG, "SWITCH_RELEASE_DETECTED");
#endif
                }
                break;
            case SWITCH_LONG_PRESS_DETECTED:
                if (value == GPIO_INPUT_LEVEL_ON) {
                    if (value_time >= button_down_time + BUTTON_HOLD_DURATION_MS) {
                        switch_state = SWITCH_HOLD_DETECTED;
                        /* callback to button_handler */
                        (*func_ptr)(&button_func_pair, SWITCH_HOLD_DETECTED);
#ifdef DEBUG_ENABLED
                        ESP_LOGI(TAG, "SWITCH_HOLD_DETECTED");
#endif
                    }
                } else {
                    switch_state = SWITCH_IDLE;
                    /* callback to button_handler */
                    (*func_ptr)(&button_func_pair, SWITCH_LONG_RELEASE_DETECTED);
#ifdef DEBUG_ENABLED
                    ESP_LOGI(TAG, "SWITCH_LONG_RELEASE_DETECTED");
#endif
                }
                break;
            case SWITCH_HOLD_DETECTED:
                if (value != GPIO_INPUT_LEVEL_ON) {
                    switch_state = SWITCH_IDLE;
                    /* callback to button_handler */
                    (*func_ptr)(&button_func_pair, SWITCH_HOLD_RELEASE_DETECTED);
#ifdef DEBUG_ENABLED
                    ESP_LOGI(TAG, "SWITCH_HOLD_RELEASE_DETECTED");
#endif
                }
                break;
            default:
                break;
            }
            if (switch_state == SWITCH_IDLE) {
                gpio_intr_enable(io_num);
                evt_flag = false;
                break;
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    }
}

/**
 * @brief init GPIO configuration as well as isr
 *
 * @param button_func_pair      pointer of the button pair.
 * @param button_num            number of button pair.
 */
static bool switch_driver_gpio_init(switch_func_pair_t *button_func_pair, uint8_t button_num)
{
    gpio_config_t io_conf = {};
    switch_func_pair = button_func_pair;
    switch_num = button_num;
    uint64_t pin_bit_mask = 0;

    /* set up button func pair pin mask */
    for (int i = 0; i < button_num; ++i) {
        pin_bit_mask |= (1ULL << (button_func_pair + i)->pin);
    }
    /* interrupt of falling edge */
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.pin_bit_mask = pin_bit_mask;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 0;
    io_conf.pull_down_en = 0;
    /* configure GPIO with the given settings */
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    /* create a queue to handle gpio event from isr */
    gpio_evt_queue = xQueueCreate(10, sizeof(switch_func_pair_t));
    if (gpio_evt_queue == 0) {
        ESP_LOGE(TAG, "Queue was not created and must not be used");
        return false;
    }
    /* install gpio isr service */
    esp_err_t err;
    /* Install the ISR service in IRAM so button and encoder edges are still
       serviced while the flash cache is disabled (e.g. Zigbee flash writes).
       All registered GPIO handlers (here and in rotary_encoder.c) are IRAM_ATTR. */
    err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_install_isr_service failed: %s", esp_err_to_name(err));
        vQueueDelete(gpio_evt_queue);
        gpio_evt_queue = NULL;
        return false;
    }
#ifdef DEBUG_ENABLED
    ESP_LOGI(TAG, "gpio_install_isr_service OK");
#endif

    for (int i = 0; i < button_num; ++i) {
        gpio_num_t pin = (button_func_pair + i)->pin;
        err = gpio_isr_handler_add(pin, gpio_isr_handler, (void *) (button_func_pair + i));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "gpio_isr_handler_add failed: %s", esp_err_to_name(err));
            /* Clean up previously added handlers */
            for (int j = 0; j < i; ++j) {
                gpio_isr_handler_remove((button_func_pair + j)->pin);
            }
            gpio_uninstall_isr_service();
            vQueueDelete(gpio_evt_queue);
            gpio_evt_queue = NULL;
            return false;
        }
#ifdef DEBUG_ENABLED
        ESP_LOGI(TAG, "gpio_isr_handler_add OK");
#endif
        /* Explicitly enable interrupt for this pin */
        gpio_intr_enable(pin);
#ifdef DEBUG_ENABLED
        ESP_LOGI(TAG, "ISR attached and enabled for GPIO %d", pin);
#endif
    }

    /* start gpio task */
    BaseType_t created = xTaskCreate(switch_driver_button_detected, "button_detected", 4096, NULL, 10, NULL);
    if (created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create button_detected task");
        /* Clean up all ISR handlers */
        for (int i = 0; i < button_num; ++i) {
            gpio_isr_handler_remove((button_func_pair + i)->pin);
        }
        gpio_uninstall_isr_service();
        vQueueDelete(gpio_evt_queue);
        gpio_evt_queue = NULL;
        return false;
    }

#ifdef DEBUG_ENABLED
    ESP_LOGI(TAG, "button_detected task created");
    ESP_LOGI(TAG, "Switch init success");

    for (int i = 0; i < switch_num; ++i) {
        gpio_num_t pin = (switch_func_pair + i)->pin;
        ESP_LOGI(TAG, "GPIO %d initial level: %d", pin, gpio_get_level(pin));
    }
#endif
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
