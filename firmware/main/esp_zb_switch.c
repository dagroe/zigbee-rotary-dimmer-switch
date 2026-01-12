/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: LicenseRef-Included
 *
 * Zigbee HA_on_off_switch Example
 *
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "string.h"

#include "esp_log.h"
#include "nvs_flash.h"
#include "ha/esp_zigbee_ha_standard.h"

#include "esp_zb_switch.h"
#include "device_config.h"
#include "switch_driver.h"
#include "rotary_encoder.h"
#include "led_driver.h"

#if defined ZB_ED_ROLE
#error Define ZB_COORDINATOR_ROLE in idf.py menuconfig to compile light switch source code.
#endif


static switch_func_pair_t button_func_pair[] = {
    {GPIO_INPUT_COMMISSION_SWITCH, SWITCH_COMMISION_CONTROL},
    {GPIO_INPUT_IO_TOGGLE_SWITCH, SWITCH_ONOFF_TOGGLE_CONTROL},
};

static QueueHandle_t led_evt_queue = NULL;

// create a flag to signal when the encoder was rotated after button downm (to prevent button up)
// static portMUX_TYPE s_encoder_mux = portMUX_INITIALIZER_UNLOCKED;
// static volatile bool encoder_rotated_flag = false;

// create a flag to signal whether the button is UP or DOWN to decie if we send BIRGHTNESS or HUE command on rotation
// static portMUX_TYPE s_button_mux = portMUX_INITIALIZER_UNLOCKED;
// static volatile bool encoder_button_down = false;

static const char *TAG = "ESP_ZB_ON_OFF_SWITCH";

static void esp_zb_buttons_handler(switch_func_pair_t *button_func_pair, switch_state_t state)
{
    if (button_func_pair->func == SWITCH_ONOFF_TOGGLE_CONTROL) {
        #ifdef DEBUG_ENABLED
        ESP_LOGI(TAG, "esp_zb_buttons_handler");
        #endif

        // bool encoder_rotated_flag_was_set;
        // taskENTER_CRITICAL(&s_encoder_mux);
        // encoder_rotated_flag_was_set = encoder_rotated_flag;
        // taskEXIT_CRITICAL(&s_encoder_mux);

        // bool button_state = false;
        switch(state) {
            case SWITCH_RELEASE_DETECTED:
                // if (encoder_rotated_flag_was_set != true) {
                    // send toggle command
                    esp_zb_zcl_on_off_cmd_t toggle_cmd_req;
                    toggle_cmd_req.zcl_basic_cmd.src_endpoint = HA_ONOFF_SWITCH_ENDPOINT;
                    toggle_cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
                    toggle_cmd_req.on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_TOGGLE_ID;
                    #ifdef DEBUG_ENABLED
                    ESP_EARLY_LOGI(TAG, "Send 'on_off toggle' command");
                    ESP_EARLY_LOGI(TAG, "Send button up");
                    #endif
                    esp_zb_zcl_on_off_cmd_req(&toggle_cmd_req);
                // }
                // set encoder button state to UP
                // button_state = false;
                break;
            case SWITCH_PRESS_DETECTED:
                // reset flag
                // taskENTER_CRITICAL(&s_encoder_mux);
                // encoder_rotated_flag = false;
                // taskEXIT_CRITICAL(&s_encoder_mux);

                // set encoder button state to DOWN
                #ifdef DEBUG_ENABLED
                ESP_EARLY_LOGI(TAG, "Send button down");
                #endif
                // button_state = true;
                break;
            case SWITCH_LONG_RELEASE_DETECTED:
                // if (encoder_rotated_flag_was_set != true) {
                    // send off command
                    esp_zb_zcl_on_off_cmd_t off_cmd_req;
                    off_cmd_req.zcl_basic_cmd.src_endpoint = HA_ONOFF_SWITCH_ENDPOINT;
                    off_cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
                    off_cmd_req.on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_OFF_ID;
                    #ifdef DEBUG_ENABLED
                    ESP_EARLY_LOGI(TAG, "Send 'on_off off' command");
                    #endif
                    esp_zb_zcl_on_off_cmd_req(&off_cmd_req);
                // }
                // set encoder button state to UP
                // button_state = false;
                break;
            case SWITCH_HOLD_RELEASE_DETECTED:
                // set encoder button state to UP
                #ifdef DEBUG_ENABLED
                ESP_EARLY_LOGI(TAG, "Send button up");
                #endif
                // button_state = false;
                break;
            default:
                break;
        }


        // Do not write button state for now, do not support push and rotate
        // We have problem to distinguish between push and rotate and push. When just pushing but the encoder fires, button up will be ignored
        //taskENTER_CRITICAL(&s_button_mux);
        //encoder_button_down = button_state;
        //taskEXIT_CRITICAL(&s_button_mux);
    }

    if (button_func_pair->func == SWITCH_COMMISION_CONTROL) {

        switch(state) {
            case SWITCH_RELEASE_DETECTED:
                break;
            case SWITCH_HOLD_DETECTED:
                // TODO: go into commission mode
                break;
            default:
                break;
        }
    }
}

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    ESP_ERROR_CHECK(esp_zb_bdb_start_top_level_commissioning(mode_mask));
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p       = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    esp_zb_zdo_signal_device_annce_params_t *dev_annce_params = NULL;

    #ifdef DEBUG_ENABLED
    ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type,
                esp_err_to_name(err_status));
    #endif

    // esp_zb_factory_reset();

    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        #ifdef DEBUG_ENABLED
        ESP_LOGI(TAG, "Zigbee stack initialized");
        #endif
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
    //         esp_zb_factory_reset();
            #ifdef DEBUG_ENABLED
            ESP_LOGI(TAG, "Device started up in %s factory-reset mode", esp_zb_bdb_is_factory_new() ? "" : "non");
            #endif
            if (esp_zb_bdb_is_factory_new()) {
                #ifdef DEBUG_ENABLED
                ESP_LOGI(TAG, "Start network steering");
                #endif
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            } else {
                #ifdef DEBUG_ENABLED
                ESP_LOGI(TAG, "Device rebooted");
                ESP_LOGI(TAG, "Start network steering");
                #endif
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            }
        } else {
            ESP_LOGE(TAG, "Failed to initialize Zigbee stack (status: %s)", esp_err_to_name(err_status));
        }
        led_state_t led_state_warn = LED_COLOR_STATE_WARN_RED;
        xQueueSend(led_evt_queue, &led_state_warn, 0);
        break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            #ifdef DEBUG_ENABLED
            ESP_LOGI(TAG, "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
                     extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                     extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                     esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
            #endif
            
        led_state_t led_state_success = LED_COLOR_STATE_BLINK_ONCE_GREEN;
        xQueueSend(led_evt_queue, &led_state_success, 0);
        } else {
            ESP_LOGE(TAG, "Network steering was not successful (status: %s)", esp_err_to_name(err_status));
            // esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
        break;
    // case ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS:
    //     if (err_status == ESP_OK) {
    //         if (*(uint8_t *)esp_zb_app_signal_get_params(p_sg_p)) {
    //             ESP_LOGI(TAG, "Network(0x%04hx) is open for %d seconds", esp_zb_get_pan_id(), *(uint8_t *)esp_zb_app_signal_get_params(p_sg_p));
    //         } else {
    //             ESP_LOGW(TAG, "Network(0x%04hx) closed, devices joining not allowed.", esp_zb_get_pan_id());
    //         }
    //     }
    //     break;
    default:
        #ifdef DEBUG_ENABLED
        ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type,
                 esp_err_to_name(err_status));
        #endif
        break;
    }
}

static void esp_zb_task(void *pvParameters)
{
    /* initialize Zigbee stack with Zigbee end-device config */
    configure_device();

    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_main_loop_iteration();
}


static void encoder_task(void *pvParameters) {
    #ifdef DEBUG_ENABLED
    ESP_LOGI(TAG, "Encoder task running");
    #endif

    // Initialise the rotary encoder device with the GPIOs for A and B signals
    rotary_encoder_info_t encoder_info = { 0 };
    ESP_ERROR_CHECK(rotary_encoder_init(&encoder_info, ROT_ENC_A_GPIO, ROT_ENC_B_GPIO));
    ESP_ERROR_CHECK(rotary_encoder_enable_half_steps(&encoder_info, ENABLE_HALF_STEPS));
#ifdef FLIP_DIRECTION
    ESP_ERROR_CHECK(rotary_encoder_flip_direction(&encoder_info));
#endif
    #ifdef DEBUG_ENABLED
    ESP_LOGI(TAG, "Stuff registered");
    #endif


    // Create a queue for events from the rotary encoder driver.
    // Tasks can read from this queue to receive up to date position information.
    QueueHandle_t encoder_event_queue = rotary_encoder_create_queue();
    ESP_ERROR_CHECK(rotary_encoder_set_queue(&encoder_info, encoder_event_queue));
    #ifdef DEBUG_ENABLED
    ESP_LOGI(TAG, "Queue created");
    #endif

    bool button_state = false;

    while (1)
    {
        // Wait for incoming events on the event queue.
        rotary_encoder_event_t event = { 0 };
        if (xQueueReceive(encoder_event_queue, &event, 1000 / portTICK_PERIOD_MS) == pdTRUE)
        {
            #ifdef DEBUG_ENABLED
            ESP_LOGI(TAG, "Event: position %lu, direction %s, button %s", event.state.position,
                     event.state.direction ? (event.state.direction == ROTARY_ENCODER_DIRECTION_CLOCKWISE ? "CW" : "CCW") : "NOT_SET",
                     button_state ? "DOWN" : "UP");
            #endif
            if (button_state) {
                // button pressed, send color command
                // TODO: try sending esp_zb_zcl_color_step_color_temperature_cmd_t
                esp_zb_zcl_color_step_hue_cmd_t cmd_req;
                cmd_req.zcl_basic_cmd.src_endpoint = HA_ONOFF_SWITCH_ENDPOINT;
                cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
                cmd_req.step_mode = event.state.direction ? (event.state.direction == ROTARY_ENCODER_DIRECTION_CLOCKWISE ? 0 : 1) : 0; // down
                cmd_req.step_size = 5;
                cmd_req.transition_time = 1;
                #ifdef DEBUG_ENABLED
                ESP_EARLY_LOGI(TAG, "Send 'color step' command");
                #endif
                esp_zb_zcl_color_step_hue_cmd_req(&cmd_req);
            } else {
                // button released, send brightness level command
                esp_zb_zcl_level_step_cmd_t cmd_req;
                cmd_req.zcl_basic_cmd.src_endpoint = HA_ONOFF_SWITCH_ENDPOINT;
                cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
                cmd_req.step_mode = event.state.direction ? (event.state.direction == ROTARY_ENCODER_DIRECTION_CLOCKWISE ? 0 : 1) : 0; // down
                cmd_req.step_size = 15;
                cmd_req.transition_time = 1;
                #ifdef DEBUG_ENABLED
                ESP_EARLY_LOGI(TAG, "Send 'level step' command");
                #endif
                esp_zb_zcl_level_step_cmd_req(&cmd_req);
            }
            // taskENTER_CRITICAL(&s_encoder_mux);
            // encoder_rotated_flag = true;
            // taskEXIT_CRITICAL(&s_encoder_mux);
            led_state_t led_state_success = LED_COLOR_STATE_BLINK_ONCE_WHITE;
            xQueueSend(led_evt_queue, &led_state_success, 0);          
        }

        // check button state queue

        // bool encoder_rotated_flag_was_set;
        // taskENTER_CRITICAL(&s_button_mux);
        // button_state = encoder_button_down;
        // taskEXIT_CRITICAL(&s_button_mux);
    }
    ESP_LOGE(TAG, "queue receive failed");

    ESP_ERROR_CHECK(rotary_encoder_uninit(&encoder_info));
}

void app_main(void) {
    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));

    // create queue to pass LED state events
    led_evt_queue = xQueueCreate(5, sizeof(led_state_t));
    if ( led_evt_queue == 0) {
        ESP_LOGE(TAG, "Queue for LED events was not created");
        return;
    }

    // create light strip
    setup_led_strip(led_evt_queue);
    set_led_rgb(16, 0, 0);

    switch_driver_init(button_func_pair, PAIR_SIZE(button_func_pair), esp_zb_buttons_handler);
    xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 4, NULL);
    xTaskCreate(encoder_task, "Ecoder_main", 4096, NULL, 5, NULL);
}
