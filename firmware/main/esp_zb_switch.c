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
#include "esp_system.h"
#include "nvs_flash.h"
#include "ha/esp_zigbee_ha_standard.h"

#include "esp_zb_switch.h"
#include "device_config.h"
#include "switch_driver.h"
#include "rotary_encoder.h"
#include "led_driver.h"
#include "ota_driver.h"
#include "relay_driver.h"

#if defined ZB_ED_ROLE
#error Define ZB_COORDINATOR_ROLE in idf.py menuconfig to compile light switch source code.
#endif


static switch_func_pair_t button_func_pair[] = {
    {GPIO_INPUT_COMMISSION_SWITCH, SWITCH_COMMISION_CONTROL},
    {GPIO_INPUT_IO_TOGGLE_SWITCH, SWITCH_ONOFF_TOGGLE_CONTROL},
    {GPIO_INPUT_RELAY_SWITCH, SWITCH_RELAY_CONTROL},
};

static QueueHandle_t led_evt_queue = NULL;

// Set true once steering succeeds, cleared on a reset-type leave. Gates outbound
// commands so we don't fire ZCL requests into the void, and drives a clear
// "not joined" LED indication.
static volatile bool s_network_joined = false;

// Shared state between button handler and encoder task for button+rotate feature
// When button is held and encoder rotates, send HUE commands instead of brightness
// When button is released after rotation, suppress the toggle command
static portMUX_TYPE s_button_encoder_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool encoder_button_down = false;
static volatile bool encoder_rotated_while_pressed = false;
static volatile TickType_t button_press_tick = 0;

// Debounce period: ignore encoder events within this time after button press
// This prevents spurious encoder events from mechanical coupling when pressing the button
#define ENCODER_DEBOUNCE_AFTER_PRESS_MS 50

// Safety timeout: if button_down state persists longer than this, force reset
// This prevents getting stuck if a release event is somehow missed
#define BUTTON_STATE_TIMEOUT_MS 5000

// ZCL step mode values (spec section 3.10.2.3.1 / 5.2.2.3.1)
#define ZCL_STEP_MODE_UP   0x00
#define ZCL_STEP_MODE_DOWN 0x01

// The ZBOSS stack is single-threaded and not reentrant: it runs on the Zigbee
// main-loop task (esp_zb_task). Any esp_zb_* call issued from another task --
// here the button-detect task (esp_zb_buttons_handler) and the encoder task --
// races the stack's internal critical sections. That corrupts FreeRTOS's
// critical-nesting counter and trips "assert failed: vPortExitCritical
// (port_uxCriticalNesting[0] > 0)" under bursts of commands (e.g. fast rotary
// turns). Guard every cross-task stack call with the stack lock.
// NOTE: callbacks that already run in stack context (esp_zb_app_signal_handler,
// scheduler-alarm callbacks) must NOT use this -- they already hold the stack
// and re-acquiring would deadlock.
//
// The wait is bounded (not portMAX_DELAY) so that if the stack ever wedges
// while holding the lock, the button/encoder tasks drop the command instead of
// blocking forever -- a frozen UI behind drywall is worse than a lost step.
#define ZB_STACK_LOCK_TIMEOUT_MS 2000
#define ZB_LOCKED(stmt) do {                                                  \
        if (esp_zb_lock_acquire(pdMS_TO_TICKS(ZB_STACK_LOCK_TIMEOUT_MS))) {   \
            stmt;                                                             \
            esp_zb_lock_release();                                           \
        } else {                                                             \
            ESP_LOGW(TAG, "Zigbee stack lock timeout, dropped: %s", #stmt);  \
        }                                                                     \
    } while (0)

// Send a bound ZCL command, but only while we're actually on a network. Either
// way the user gets LED feedback: a white blink when the command is sent, a red
// blink when it's dropped because the device isn't joined.
#define ZB_SEND_CMD(stmt) do {                                       \
        led_state_t _fb;                                             \
        if (s_network_joined) {                                      \
            ZB_LOCKED(stmt);                                         \
            _fb = LED_COLOR_STATE_BLINK_ONCE_WHITE;                  \
        } else {                                                     \
            ESP_LOGW(TAG, "Not joined; dropped: %s", #stmt);         \
            _fb = LED_COLOR_STATE_BLINK_ONCE_RED;                    \
        }                                                            \
        xQueueSend(led_evt_queue, &_fb, 0);                          \
    } while (0)

static const char *TAG = "ESP_ZB_ON_OFF_SWITCH";

static void esp_zb_buttons_handler(switch_func_pair_t *button_func_pair, switch_state_t state)
{
    if (button_func_pair->func == SWITCH_ONOFF_TOGGLE_CONTROL) {
        #ifdef DEBUG_ENABLED
        ESP_LOGI(TAG, "esp_zb_buttons_handler state=%d", state);
        #endif

        switch(state) {
            case SWITCH_PRESS_DETECTED:
                // Button pressed: record time and reset rotation flag
                taskENTER_CRITICAL(&s_button_encoder_mux);
                encoder_button_down = true;
                encoder_rotated_while_pressed = false;
                button_press_tick = xTaskGetTickCount();
                taskEXIT_CRITICAL(&s_button_encoder_mux);
                #ifdef DEBUG_ENABLED
                ESP_LOGI(TAG, "Button down, encoder_button_down=true");
                #endif
                break;

            case SWITCH_RELEASE_DETECTED: {
                // Short press release: send toggle only if encoder didn't rotate
                bool rotated;
                taskENTER_CRITICAL(&s_button_encoder_mux);
                rotated = encoder_rotated_while_pressed;
                encoder_button_down = false;
                encoder_rotated_while_pressed = false;
                taskEXIT_CRITICAL(&s_button_encoder_mux);

                if (!rotated) {
                    esp_zb_zcl_on_off_cmd_t toggle_cmd_req;
                    toggle_cmd_req.zcl_basic_cmd.src_endpoint = HA_ONOFF_SWITCH_ENDPOINT;
                    toggle_cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
                    toggle_cmd_req.on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_TOGGLE_ID;
                    #ifdef DEBUG_ENABLED
                    ESP_LOGI(TAG, "Send 'on_off toggle' command");
                    #endif
                    ZB_SEND_CMD(esp_zb_zcl_on_off_cmd_req(&toggle_cmd_req));
                } else {
                    #ifdef DEBUG_ENABLED
                    ESP_LOGI(TAG, "Toggle suppressed - encoder rotated while pressed");
                    #endif
                }
                break;
            }

            case SWITCH_LONG_RELEASE_DETECTED: {
                // Long press release: send off only if encoder didn't rotate
                bool rotated;
                taskENTER_CRITICAL(&s_button_encoder_mux);
                rotated = encoder_rotated_while_pressed;
                encoder_button_down = false;
                encoder_rotated_while_pressed = false;
                taskEXIT_CRITICAL(&s_button_encoder_mux);

                if (!rotated) {
                    esp_zb_zcl_on_off_cmd_t off_cmd_req;
                    off_cmd_req.zcl_basic_cmd.src_endpoint = HA_ONOFF_SWITCH_ENDPOINT;
                    off_cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
                    off_cmd_req.on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_OFF_ID;
                    #ifdef DEBUG_ENABLED
                    ESP_LOGI(TAG, "Send 'on_off off' command");
                    #endif
                    ZB_SEND_CMD(esp_zb_zcl_on_off_cmd_req(&off_cmd_req));
                } else {
                    #ifdef DEBUG_ENABLED
                    ESP_LOGI(TAG, "Off suppressed - encoder rotated while pressed");
                    #endif
                }
                break;
            }

            case SWITCH_LONG_PRESS_DETECTED:
                // Long press detected (button still held): no action needed, state stays
                #ifdef DEBUG_ENABLED
                ESP_LOGI(TAG, "Long press detected, button still held");
                #endif
                break;

            case SWITCH_HOLD_DETECTED:
                // Hold detected (button still held): no action needed, state stays
                #ifdef DEBUG_ENABLED
                ESP_LOGI(TAG, "Hold detected, button still held");
                #endif
                break;

            case SWITCH_HOLD_RELEASE_DETECTED:
                // Hold release (after very long press): just clear button state
                taskENTER_CRITICAL(&s_button_encoder_mux);
                encoder_button_down = false;
                encoder_rotated_while_pressed = false;
                taskEXIT_CRITICAL(&s_button_encoder_mux);
                #ifdef DEBUG_ENABLED
                ESP_LOGI(TAG, "Hold release, encoder_button_down=false");
                #endif
                break;

            default:
                // Safety: any unknown state should clear button_down to prevent stuck state
                taskENTER_CRITICAL(&s_button_encoder_mux);
                encoder_button_down = false;
                encoder_rotated_while_pressed = false;
                taskEXIT_CRITICAL(&s_button_encoder_mux);
                #ifdef DEBUG_ENABLED
                ESP_LOGI(TAG, "Unknown state %d, clearing button state", state);
                #endif
                break;
        }
    }

    if (button_func_pair->func == SWITCH_COMMISION_CONTROL) {

        switch(state) {
            case SWITCH_RELEASE_DETECTED:
                // Short press: trigger network steering (rejoin/find new network)
                ESP_LOGI(TAG, "Commission button: start network steering");
                ZB_LOCKED(esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING));
                {
                    led_state_t led_state = LED_COLOR_STATE_BLINK_ONCE_YELLOW;
                    xQueueSend(led_evt_queue, &led_state, 0);
                }
                break;
            case SWITCH_LONG_PRESS_DETECTED:
                // Held past 0.5s: warn (blinking yellow) that continuing to ~5s
                // will factory-reset, so it can't happen silently/accidentally.
                ESP_LOGW(TAG, "Hold to factory reset (release now to abort)...");
                {
                    led_state_t led_state = LED_COLOR_STATE_BLINK_YELLOW;
                    xQueueSend(led_evt_queue, &led_state, 0);
                }
                break;
            case SWITCH_LONG_RELEASE_DETECTED:
                // Released during the warning window: abort, no reset, no steer.
                ESP_LOGI(TAG, "Factory reset aborted");
                {
                    led_state_t led_state = s_network_joined ? LED_COLOR_STATE_OFF
                                                             : LED_COLOR_STATE_WARN_RED;
                    xQueueSend(led_evt_queue, &led_state, 0);
                }
                break;
            case SWITCH_HOLD_DETECTED:
                // Held to 5s: factory reset
                ESP_LOGW(TAG, "Commission button held: factory reset");
                {
                    led_state_t led_state = LED_COLOR_STATE_BLINK_RED;
                    xQueueSend(led_evt_queue, &led_state, 0);
                }
                ZB_LOCKED(esp_zb_factory_reset());
                break;
            default:
                break;
        }
    }

    if (button_func_pair->func == SWITCH_RELAY_CONTROL) {
        // Dedicated relay button. Works regardless of network state (this is the
        // local/offline cut-off), and we reflect the new outlet state back to the
        // coordinator's on/off attribute so HA stays in sync.
        if (state == SWITCH_RELEASE_DETECTED) {
            bool outlet_on = !relay_get();
            relay_set(outlet_on);
            uint8_t attr_val = outlet_on ? 1 : 0;
            ZB_LOCKED(esp_zb_zcl_set_attribute_val(HA_RELAY_ENDPOINT,
                      ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                      ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, &attr_val, false));
            ESP_LOGI(TAG, "Relay button: outlet %s", outlet_on ? "POWERED" : "CUT");
            led_state_t led = LED_COLOR_STATE_BLINK_ONCE_BLUE;  // distinct from light commands (white)
            xQueueSend(led_evt_queue, &led, 0);
        }
    }
}

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    /* Don't ESP_ERROR_CHECK here: a re-steer can legitimately return an error
     * (e.g. one is already in progress). Aborting would reboot the device. */
    esp_err_t err = esp_zb_bdb_start_top_level_commissioning(mode_mask);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "start_top_level_commissioning(0x%x) failed: %s", mode_mask, esp_err_to_name(err));
    }
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p       = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;

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
            
        s_network_joined = true;
        led_state_t led_state_success = LED_COLOR_STATE_BLINK_ONCE_GREEN;
        xQueueSend(led_evt_queue, &led_state_success, 0);

        /* Joining proves the stack/radio work: confirm a pending OTA image so it
         * isn't rolled back. No-op unless this boot is a freshly-OTA'd image. */
        ota_confirm_running_image();
        } else {
            ESP_LOGW(TAG, "Network steering was not successful (status: %s), retrying in 3s", esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 3000);
        }
        break;
    case ESP_ZB_ZDO_SIGNAL_LEAVE: {
        /* Track connection state + LED ONLY -- do NOT call commissioning APIs
         * here: re-steering from the leave handler raced the stack's leave/reset
         * state machine and asserted in zdo_app.c (see git history). A
         * leave-with-REJOIN is the stack reconnecting itself, so ignore it; only
         * a leave-with-RESET means we were actually removed from the network. We
         * deliberately do NOT clear the flag on transient link loss, because a
         * router auto-rejoins without necessarily re-emitting a steering signal,
         * which would leave commands stuck "suppressed". */
        esp_zb_zdo_signal_leave_params_t *leave_params =
            (esp_zb_zdo_signal_leave_params_t *)esp_zb_app_signal_get_params(p_sg_p);
        if (leave_params && leave_params->leave_type == ESP_ZB_NWK_LEAVE_TYPE_REJOIN) {
            break;
        }
        s_network_joined = false;
        ESP_LOGW(TAG, "Removed from network (reset leave); commands suppressed until re-paired");
        led_state_t led_state_lost = LED_COLOR_STATE_WARN_RED;
        xQueueSend(led_evt_queue, &led_state_lost, 0);
        break;
    }
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

/* Dispatcher for Zigbee core action callbacks. Currently only OTA needs one. */
static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message)
{
    switch (callback_id) {
    case ESP_ZB_CORE_OTA_UPGRADE_VALUE_CB_ID:
        return ota_handle_value((const esp_zb_zcl_ota_upgrade_value_message_t *)message);
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID: {
        const esp_zb_zcl_set_attr_value_message_t *m =
            (const esp_zb_zcl_set_attr_value_message_t *)message;
        /* Coordinator wrote the relay endpoint's On/Off attribute -> drive GPIO. */
        if (m->info.dst_endpoint == HA_RELAY_ENDPOINT &&
            m->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF &&
            m->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID &&
            m->attribute.data.value != NULL) {
            relay_set(*(bool *)m->attribute.data.value);
        }
        return ESP_OK;
    }
    default:
        #ifdef DEBUG_ENABLED
        ESP_LOGI(TAG, "Unhandled Zigbee action callback 0x%x", callback_id);
        #endif
        return ESP_OK;
    }
}

static void esp_zb_task(void *pvParameters)
{
    /* initialize Zigbee stack with Zigbee end-device config */
    configure_device();

    esp_zb_core_action_handler_register(zb_action_handler);

    ESP_ERROR_CHECK(esp_zb_start(false));
    ota_client_start(HA_ONOFF_SWITCH_ENDPOINT);
    esp_zb_stack_main_loop();
}


static void encoder_task(void *pvParameters) {
    #ifdef DEBUG_ENABLED
    ESP_LOGI(TAG, "Encoder task running");
    #endif

    // Initialise the rotary encoder device with the GPIOs for A and B signals
    rotary_encoder_info_t encoder_info = { 0 };
    ESP_ERROR_CHECK(rotary_encoder_init(&encoder_info, ROT_ENC_A_GPIO, ROT_ENC_B_GPIO));
    ESP_ERROR_CHECK(rotary_encoder_enable_half_steps(&encoder_info, ENABLE_HALF_STEPS));
#if FLIP_DIRECTION
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

    while (1)
    {
        // Safety check: periodically verify button state isn't stuck
        // This runs every iteration (every 1000ms timeout or when event received)
        {
            TickType_t now = xTaskGetTickCount();
            taskENTER_CRITICAL(&s_button_encoder_mux);
            if (encoder_button_down) {
                TickType_t elapsed_ticks = now - button_press_tick;
                if (elapsed_ticks > pdMS_TO_TICKS(BUTTON_STATE_TIMEOUT_MS)) {
                    // Button state has been "down" for too long - force reset
                    encoder_button_down = false;
                    encoder_rotated_while_pressed = false;
                    taskEXIT_CRITICAL(&s_button_encoder_mux);
                    ESP_LOGW(TAG, "Button state timeout after %lu ms - force reset",
                             (unsigned long)(elapsed_ticks * portTICK_PERIOD_MS));
                } else {
                    taskEXIT_CRITICAL(&s_button_encoder_mux);
                }
            } else {
                taskEXIT_CRITICAL(&s_button_encoder_mux);
            }
        }

        // Wait for incoming events on the event queue.
        rotary_encoder_event_t event = { 0 };
        if (xQueueReceive(encoder_event_queue, &event, 1000 / portTICK_PERIOD_MS) == pdTRUE)
        {
            // Read button state and press time atomically
            bool button_down;
            TickType_t press_tick;
            taskENTER_CRITICAL(&s_button_encoder_mux);
            button_down = encoder_button_down;
            press_tick = button_press_tick;
            taskEXIT_CRITICAL(&s_button_encoder_mux);

            #ifdef DEBUG_ENABLED
            ESP_LOGI(TAG, "Event: position %lu, direction %s, button %s", event.state.position,
                     event.state.direction ? (event.state.direction == ROTARY_ENCODER_DIRECTION_CLOCKWISE ? "CW" : "CCW") : "NOT_SET",
                     button_down ? "DOWN" : "UP");
            #endif

            if (button_down) {
                // Button is pressed - check if we're past the debounce period
                TickType_t now = xTaskGetTickCount();
                TickType_t elapsed_ticks = now - press_tick;
                TickType_t debounce_ticks = pdMS_TO_TICKS(ENCODER_DEBOUNCE_AFTER_PRESS_MS);

                if (elapsed_ticks >= debounce_ticks) {
                    // Past debounce period - this is intentional rotation while button held
                    // Mark that rotation occurred (to suppress toggle on release)
                    taskENTER_CRITICAL(&s_button_encoder_mux);
                    encoder_rotated_while_pressed = true;
                    taskEXIT_CRITICAL(&s_button_encoder_mux);

                    // Push + rotate = color temperature (warm/cool). Works on
                    // tunable-white bulbs; CW = warmer (higher mireds).
                    esp_zb_zcl_color_step_color_temperature_cmd_t cmd_req;
                    cmd_req.zcl_basic_cmd.src_endpoint = HA_ONOFF_SWITCH_ENDPOINT;
                    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
                    cmd_req.move_mode = event.state.direction == ROTARY_ENCODER_DIRECTION_CLOCKWISE
                                        ? ESP_ZB_ZCL_CMD_COLOR_CONTROL_STEP_UP
                                        : ESP_ZB_ZCL_CMD_COLOR_CONTROL_STEP_DOWN;
                    cmd_req.step_size = 25;                  // mireds per detent
                    cmd_req.transition_time = 1;
                    cmd_req.color_temperature_minimum = 153; // ~6500 K (cool)
                    cmd_req.color_temperature_maximum = 500; // ~2000 K (warm)
                    #ifdef DEBUG_ENABLED
                    ESP_LOGI(TAG, "Send 'color temperature step' command (button held)");
                    #endif
                    ZB_SEND_CMD(esp_zb_zcl_color_step_color_temperature_cmd_req(&cmd_req));
                } else {
                    // Within debounce period - ignore this encoder event (likely noise from button press)
                    #ifdef DEBUG_ENABLED
                    ESP_LOGI(TAG, "Encoder event ignored (within debounce period, %lu < %lu ticks)",
                             (unsigned long)elapsed_ticks, (unsigned long)debounce_ticks);
                    #endif
                }
            } else {
                // Button not pressed - send brightness level command
                esp_zb_zcl_level_step_cmd_t cmd_req;
                cmd_req.zcl_basic_cmd.src_endpoint = HA_ONOFF_SWITCH_ENDPOINT;
                cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
                cmd_req.step_mode = event.state.direction == ROTARY_ENCODER_DIRECTION_CLOCKWISE ? ZCL_STEP_MODE_UP : ZCL_STEP_MODE_DOWN;
                cmd_req.step_size = 15;
                cmd_req.transition_time = 1;
                #ifdef DEBUG_ENABLED
                ESP_LOGI(TAG, "Send 'level step' command");
                #endif
                ZB_SEND_CMD(esp_zb_zcl_level_step_cmd_req(&cmd_req));
            }
        }
    }
}

void app_main(void) {
    /* Put the 230V relay in a known-OFF state as the very first thing at boot. */
    relay_init();

    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));

    // create queue to pass LED state events
    led_evt_queue = xQueueCreate(5, sizeof(led_state_t));
    if ( led_evt_queue == 0) {
        // Wall-installed device: a half-initialized state is unrecoverable and
        // there is no one to power-cycle it, so reboot and let init retry.
        ESP_LOGE(TAG, "Queue for LED events was not created, rebooting");
        esp_restart();
    }

    // create light strip
    setup_led_strip(led_evt_queue);
    led_state_t led_state_init = LED_COLOR_STATE_WARN_RED;
    xQueueSend(led_evt_queue, &led_state_init, 0);

    if (!switch_driver_init(button_func_pair, PAIR_SIZE(button_func_pair), esp_zb_buttons_handler)) {
        ESP_LOGE(TAG, "Switch driver init failed, rebooting");
        esp_restart();
    }
    // Zigbee task runs the stack/radio timers; it must not be starved by the
    // higher-priority UI tasks (button=10, LED=6, encoder=5) under a fast
    // encoder burst, so give it priority 5 and extra stack headroom.
    if (xTaskCreate(esp_zb_task, "Zigbee_main", 6144, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Zigbee task, rebooting");
        esp_restart();
    }
    if (xTaskCreate(encoder_task, "Encoder_main", 4096, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create encoder task, rebooting");
        esp_restart();
    }
}
