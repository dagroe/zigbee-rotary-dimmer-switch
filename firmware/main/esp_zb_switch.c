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

#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "esp_zb_switch.h"
#include "led_strip.h"


#include "rotary_encoder.h"

#if defined ZB_ED_ROLE
#error Define ZB_COORDINATOR_ROLE in idf.py menuconfig to compile light switch source code.
#endif



/// LED strip common configuration
led_strip_config_t strip_config = {
    .strip_gpio_num = LED_GPIO,  // The GPIO that connected to the LED strip's data line
    .max_leds = 1,                 // The number of LEDs in the strip,
    // .led_model = LED_MODEL_WS2812, // LED strip model, it determines the bit timing
    // .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB, // The color component format is G-R-B
    // .flags = {
    //     .invert_out = false, // don't invert the output signal
    // }
};

/// RMT backend specific configuration
led_strip_rmt_config_t rmt_config = {
    // .clk_src = RMT_CLK_SRC_DEFAULT,    // different clock source can lead to different power consumption
    .resolution_hz = 10 * 1000 * 1000, // RMT counter clock frequency: 10MHz
    // .mem_block_symbols = 64,           // the memory size of each RMT channel, in words (4 bytes)
    .flags = {
        .with_dma = false, // DMA feature is available on chips like ESP32-S3/P4
    }
};

/// Create the LED strip object
led_strip_handle_t led_strip;


typedef struct light_bulb_device_params_s {
    esp_zb_ieee_addr_t ieee_addr;
    uint8_t  endpoint;
    uint16_t short_addr;
} light_bulb_device_params_t;

static switch_func_pair_t button_func_pair[] = {
    {GPIO_INPUT_COMMISSION_SWITCH, SWITCH_COMMISION_CONTROL},
    {GPIO_INPUT_IO_TOGGLE_SWITCH, SWITCH_ONOFF_TOGGLE_CONTROL},
};

static QueueHandle_t encoder_btn_evt_queue = NULL;
static QueueHandle_t led_evt_queue = NULL;

static const char *TAG = "ESP_ZB_ON_OFF_SWITCH";

static void esp_zb_buttons_handler(switch_func_pair_t *button_func_pair, switch_state_t state)
{
    if (button_func_pair->func == SWITCH_ONOFF_TOGGLE_CONTROL) {
        ESP_LOGI(TAG, "esp_zb_buttons_handler");
        bool button_state = false;
        switch(state) {
            case SWITCH_RELEASE_DETECTED:
                // send toggle command
                esp_zb_zcl_on_off_cmd_t toggle_cmd_req;
                toggle_cmd_req.zcl_basic_cmd.src_endpoint = HA_ONOFF_SWITCH_ENDPOINT;
                toggle_cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
                toggle_cmd_req.on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_TOGGLE_ID;
                ESP_EARLY_LOGI(TAG, "Send 'on_off toggle' command");
                ESP_EARLY_LOGI(TAG, "Send button up");
                esp_zb_zcl_on_off_cmd_req(&toggle_cmd_req);
                // set encoder button state to UP
                button_state = false;
                xQueueSend(encoder_btn_evt_queue, &button_state, 0);
                break;
            case SWITCH_PRESS_DETECTED:
                // set encoder button state to DOWN
                ESP_EARLY_LOGI(TAG, "Send button down");
                button_state = true;
                xQueueSend(encoder_btn_evt_queue, &button_state, 0);
                break;
            case SWITCH_LONG_RELEASE_DETECTED:
                // send off command
                esp_zb_zcl_on_off_cmd_t off_cmd_req;
                off_cmd_req.zcl_basic_cmd.src_endpoint = HA_ONOFF_SWITCH_ENDPOINT;
                off_cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
                off_cmd_req.on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_OFF_ID;
                ESP_EARLY_LOGI(TAG, "Send 'on_off off' command");
                esp_zb_zcl_on_off_cmd_req(&off_cmd_req);
                break;
            case SWITCH_HOLD_RELEASE_DETECTED:
                // set encoder button state to UP
                ESP_EARLY_LOGI(TAG, "Send button up");
                button_state = false;
                xQueueSend(encoder_btn_evt_queue, &button_state, 0);
                break;
            default:
                break;
        }
        /* implemented light switch toggle functionality */
        
        // esp_zb_zcl_level_step_cmd_t cmd_req;
        // cmd_req.zcl_basic_cmd.src_endpoint = HA_ONOFF_SWITCH_ENDPOINT;
        // cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
        // cmd_req.step_mode = 1; // down
        // cmd_req.step_size = 5;
        // cmd_req.transition_time = 10;
        // ESP_EARLY_LOGI(TAG, "Send 'level step' command");
        // esp_zb_zcl_level_step_cmd_req(&cmd_req);
    }
    if (button_func_pair->func == SWITCH_COMMISION_CONTROL) {

        switch(state) {
            case SWITCH_RELEASE_DETECTED:
                // send toggle command
                esp_zb_zcl_on_off_cmd_t toggle_cmd_req2;
                toggle_cmd_req2.zcl_basic_cmd.src_endpoint = HA_ONOFF_SWITCH_ENDPOINT;
                toggle_cmd_req2.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
                toggle_cmd_req2.on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_TOGGLE_ID;
                ESP_EARLY_LOGI(TAG, "Send 'on_off toggle' command");
                esp_zb_zcl_on_off_cmd_req(&toggle_cmd_req2);
            case SWITCH_HOLD_DETECTED:
                // TODO: go into commission mode
                // esp_zb_zcl_on_off_cmd_t cmd_req;
                // cmd_req.zcl_basic_cmd.src_endpoint = HA_ONOFF_SWITCH_ENDPOINT;
                // cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
                // cmd_req.on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_TOGGLE_ID;
                // ESP_EARLY_LOGI(TAG, "Send 'on_off toggle' command");
                // esp_zb_zcl_on_off_cmd_req(&cmd_req);
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

static void bind_cb(esp_zb_zdp_status_t zdo_status, void *user_ctx)
{
    if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
        ESP_LOGI(TAG, "Bound successfully!");
        if (user_ctx) {
            light_bulb_device_params_t *light = (light_bulb_device_params_t *)user_ctx;
            ESP_LOGI(TAG, "The light originating from address(0x%x) on endpoint(%d)", light->short_addr, light->endpoint);
            free(light);
        }
    }
}

static void user_find_cb(esp_zb_zdp_status_t zdo_status, uint16_t addr, uint8_t endpoint, void *user_ctx)
{
    if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
        ESP_LOGI(TAG, "Found light");
        esp_zb_zdo_bind_req_param_t bind_req;
        light_bulb_device_params_t *light = (light_bulb_device_params_t *)malloc(sizeof(light_bulb_device_params_t));
        light->endpoint = endpoint;
        light->short_addr = addr;
        esp_zb_ieee_address_by_short(light->short_addr, light->ieee_addr);
        esp_zb_get_long_address(bind_req.src_address);
        bind_req.src_endp = HA_ONOFF_SWITCH_ENDPOINT;
        bind_req.cluster_id = ESP_ZB_ZCL_CLUSTER_ID_ON_OFF;
        bind_req.dst_addr_mode = ESP_ZB_ZDO_BIND_DST_ADDR_MODE_64_BIT_EXTENDED;
        memcpy(bind_req.dst_address_u.addr_long, light->ieee_addr, sizeof(esp_zb_ieee_addr_t));
        bind_req.dst_endp = endpoint;
        bind_req.req_dst_addr = esp_zb_get_short_address();
        ESP_LOGI(TAG, "Try to bind On/Off");
        esp_zb_zdo_device_bind_req(&bind_req, bind_cb, (void *)light);
    }
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p       = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    esp_zb_zdo_signal_device_annce_params_t *dev_annce_params = NULL;

    ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type,
                esp_err_to_name(err_status));

    // esp_zb_factory_reset();

    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Zigbee stack initialized");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
    //         esp_zb_factory_reset();
            ESP_LOGI(TAG, "Device started up in %s factory-reset mode", esp_zb_bdb_is_factory_new() ? "" : "non");
            if (esp_zb_bdb_is_factory_new()) {
                ESP_LOGI(TAG, "Start network steering");
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            } else {
                ESP_LOGI(TAG, "Device rebooted");
                ESP_LOGI(TAG, "Start network steering");
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
            ESP_LOGI(TAG, "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
                     extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                     extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                     esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
            
        led_state_t led_state_success = LED_COLOR_STATE_OK_GREEN;
        xQueueSend(led_evt_queue, &led_state_success, 0);
        } else {
            ESP_LOGI(TAG, "Network steering was not successful (status: %s)", esp_err_to_name(err_status));
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
        ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type,
                 esp_err_to_name(err_status));
        break;
    }
}

char modelid[] = { 4, 'R','T','0','2' };
char manufname[] = { 14, 'D','G',' ','E','l','e','c','t','r','o','n','i','c','s' };
char sw_build_version[] = { 9, '2','0','2','4','0','6','2','8','1' };

static void esp_zb_task(void *pvParameters)
{
    /* initialize Zigbee stack with Zigbee end-device config */
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZC_CONFIG();
    esp_zb_init(&zb_nwk_cfg);
    esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);

    /* set the on-off light device config */
    uint8_t zcl_version, power_source_id, identify_time;
 
    zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE;
    power_source_id = 0x01;//zb_zcl_basic_power_source_e.ZB_ZCL_BASIC_POWER_SOURCE_MAINS_SINGLE_PHASE;
    identify_time = ESP_ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE;
    /* basic cluster create with fully customized */
    esp_zb_attribute_list_t *esp_zb_basic_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_BASIC);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_ZCL_VERSION_ID, &zcl_version);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID, &power_source_id);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, &modelid[0]);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, &manufname[0]);
    /* identify cluster create with fully customized */
    esp_zb_attribute_list_t *esp_zb_identify_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY);
    esp_zb_identify_cluster_add_attr(esp_zb_identify_cluster, ESP_ZB_ZCL_ATTR_IDENTIFY_IDENTIFY_TIME_ID, &identify_time);
    /* group cluster create with fully customized */
    // esp_zb_attribute_list_t *esp_zb_groups_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_GROUPS);
    // esp_zb_groups_cluster_add_attr(esp_zb_groups_cluster, ESP_ZB_ZCL_ATTR_GROUPS_NAME_SUPPORT_ID, &test_attr);
    /* scenes cluster create with standard cluster + customized */
    // esp_zb_attribute_list_t *esp_zb_scenes_cluster = esp_zb_scenes_cluster_create(NULL);
    // esp_zb_cluster_update_attr(esp_zb_scenes_cluster, ESP_ZB_ZCL_ATTR_SCENES_NAME_SUPPORT_ID, &test_attr);

    /* on-off cluster create with standard cluster config*/
    esp_zb_on_off_cluster_cfg_t on_off_cfg;
    on_off_cfg.on_off = ESP_ZB_ZCL_ON_OFF_ON_OFF_DEFAULT_VALUE;
    esp_zb_attribute_list_t *esp_zb_on_off_cluster = esp_zb_on_off_cluster_create(&on_off_cfg);

    // switch cluster
    esp_zb_on_off_switch_cluster_cfg_t on_off_switch_cfg;
    on_off_switch_cfg.switch_type = ESP_ZB_ZCL_ON_OFF_SWITCH_CONFIGURATION_SWITCH_TYPE_MULTIFUNCTION;
    on_off_switch_cfg.switch_action = ESP_ZB_ZCL_ON_OFF_SWITCH_CONFIGURATION_SWITCH_ACTIONS_TOGGLE;
    esp_zb_attribute_list_t *esp_zb_on_off_switch_cluster = esp_zb_on_off_switch_cfg_cluster_create(&on_off_switch_cfg);

    // level control cluster
    esp_zb_level_cluster_cfg_t level_control_cfg;
    level_control_cfg.current_level = ESP_ZB_ZCL_LEVEL_CONTROL_CURRENT_LEVEL_DEFAULT_VALUE;
    esp_zb_attribute_list_t *esp_zb_level_control_cluster = esp_zb_level_cluster_create(&level_control_cfg);

    // color control
    esp_zb_color_cluster_cfg_t color_control_cfg;
    color_control_cfg.current_x = ESP_ZB_ZCL_COLOR_CONTROL_CURRENT_X_DEF_VALUE;
    color_control_cfg.current_y = ESP_ZB_ZCL_COLOR_CONTROL_CURRENT_Y_DEF_VALUE;
    color_control_cfg.color_mode = ESP_ZB_ZCL_COLOR_CONTROL_COLOR_MODE_DEFAULT_VALUE;
    color_control_cfg.options = ESP_ZB_ZCL_COLOR_CONTROL_OPTIONS_DEFAULT_VALUE;
    color_control_cfg.enhanced_color_mode = ESP_ZB_ZCL_COLOR_CONTROL_ENHANCED_COLOR_MODE_DEFAULT_VALUE;
    color_control_cfg.color_capabilities = ESP_ZB_ZCL_COLOR_CONTROL_COLOR_CAPABILITIES_DEFAULT_VALUE;
    esp_zb_attribute_list_t *esp_zb_color_control_cluster = esp_zb_color_control_cluster_create(&color_control_cfg);

    /* create cluster lists for this endpoint */
    esp_zb_cluster_list_t *esp_zb_cluster_list = esp_zb_zcl_cluster_list_create();
    esp_zb_cluster_list_add_basic_cluster(esp_zb_cluster_list, esp_zb_basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    /* update basic cluster in the existed cluster list */
    //esp_zb_cluster_list_update_basic_cluster(esp_zb_cluster_list, esp_zb_basic_cluster_create(NULL), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_identify_cluster(esp_zb_cluster_list, esp_zb_identify_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    // esp_zb_cluster_list_add_groups_cluster(esp_zb_cluster_list, esp_zb_groups_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    // esp_zb_cluster_list_add_scenes_cluster(esp_zb_cluster_list, esp_zb_scenes_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_on_off_cluster(esp_zb_cluster_list, esp_zb_on_off_cluster, ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);
    esp_zb_cluster_list_add_on_off_switch_config_cluster(esp_zb_cluster_list, esp_zb_on_off_switch_cluster, ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);
    esp_zb_cluster_list_add_level_cluster(esp_zb_cluster_list, esp_zb_level_control_cluster, ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);
    esp_zb_cluster_list_add_color_control_cluster(esp_zb_cluster_list, esp_zb_color_control_cluster, ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);

    esp_zb_ep_list_t *esp_zb_ep_list = esp_zb_ep_list_create();
    /* add created endpoint (cluster_list) to endpoint list */
    esp_zb_ep_list_add_ep(esp_zb_ep_list, esp_zb_cluster_list, HA_ONOFF_SWITCH_ENDPOINT, ESP_ZB_AF_HA_PROFILE_ID, ESP_ZB_HA_ON_OFF_SWITCH_DEVICE_ID);
    esp_zb_device_register(esp_zb_ep_list);
    // esp_zb_core_action_handler_register(zb_action_handler);
    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_main_loop_iteration();
}


static void encoder_task(void *pvParameters) {
    ESP_LOGI(TAG, "Encoder task running");

    // Initialise the rotary encoder device with the GPIOs for A and B signals
    rotary_encoder_info_t encoder_info = { 0 };
    ESP_ERROR_CHECK(rotary_encoder_init(&encoder_info, ROT_ENC_A_GPIO, ROT_ENC_B_GPIO));
    ESP_ERROR_CHECK(rotary_encoder_enable_half_steps(&encoder_info, ENABLE_HALF_STEPS));
#ifdef FLIP_DIRECTION
    ESP_ERROR_CHECK(rotary_encoder_flip_direction(&encoder_info));
#endif
    ESP_LOGI(TAG, "Stuff registered");


    // Create a queue for events from the rotary encoder driver.
    // Tasks can read from this queue to receive up to date position information.
    QueueHandle_t encoder_event_queue = rotary_encoder_create_queue();
    ESP_ERROR_CHECK(rotary_encoder_set_queue(&encoder_info, encoder_event_queue));
    ESP_LOGI(TAG, "Queue created");

    bool button_state = false;

    while (1)
    {
        // Wait for incoming events on the event queue.
        rotary_encoder_event_t event = { 0 };
        if (xQueueReceive(encoder_event_queue, &event, 1000 / portTICK_PERIOD_MS) == pdTRUE)
        {
            ESP_LOGI(TAG, "Event: position %lu, direction %s, button %s", event.state.position,
                     event.state.direction ? (event.state.direction == ROTARY_ENCODER_DIRECTION_CLOCKWISE ? "CW" : "CCW") : "NOT_SET",
                     button_state ? "DOWN" : "UP");
            if (button_state) {
                // button pressed, send color command
                // TODO: try sending esp_zb_zcl_color_step_color_temperature_cmd_t
                esp_zb_zcl_color_step_hue_cmd_t cmd_req;
                cmd_req.zcl_basic_cmd.src_endpoint = HA_ONOFF_SWITCH_ENDPOINT;
                cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
                cmd_req.step_mode = event.state.direction ? (event.state.direction == ROTARY_ENCODER_DIRECTION_CLOCKWISE ? 0 : 1) : 0; // down
                cmd_req.step_size = 5;
                cmd_req.transition_time = 1;
                ESP_EARLY_LOGI(TAG, "Send 'color step' command");
                esp_zb_zcl_color_step_hue_cmd_req(&cmd_req);
            } else {
                // button released, send brightness level command
                esp_zb_zcl_level_step_cmd_t cmd_req;
                cmd_req.zcl_basic_cmd.src_endpoint = HA_ONOFF_SWITCH_ENDPOINT;
                cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
                cmd_req.step_mode = event.state.direction ? (event.state.direction == ROTARY_ENCODER_DIRECTION_CLOCKWISE ? 0 : 1) : 0; // down
                cmd_req.step_size = 15;
                cmd_req.transition_time = 1;
                ESP_EARLY_LOGI(TAG, "Send 'level step' command");
                esp_zb_zcl_level_step_cmd_req(&cmd_req);
            }            
        }

        // check button state queue
        xQueueReceive(encoder_btn_evt_queue, &button_state, 10 / portTICK_PERIOD_MS);
    }
    ESP_LOGE(TAG, "queue receive failed");

    ESP_ERROR_CHECK(rotary_encoder_uninit(&encoder_info));
}

static void led_task(void *pvParameters) {
    ESP_LOGI(TAG, "LED task running");

    led_state_t current_state = LED_COLOR_STATE_OFF;

    while (1)
    {
        // Wait for incoming events on the event queue.
        led_state_t new_state = LED_COLOR_STATE_OFF;
        if (xQueueReceive(led_evt_queue, &new_state, 1000 / portTICK_PERIOD_MS) == pdTRUE && new_state != current_state)
        {
            ESP_LOGI(TAG, "LED event: code %u", new_state);
            switch (new_state)
            {
            case LED_COLOR_STATE_WARN_RED:
                ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 0, 50, 0, 0));
                break;
            case LED_COLOR_STATE_OK_GREEN:
                ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 0, 0, 50, 0));
                break;
            case LED_COLOR_STATE_FEEDBACK_WHITE_ONE_PULSE:
                ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 0, 50, 50, 50));
                break;
            case LED_COLOR_STATE_WATITNG_YELLOW_BLINK:
                // TODO: implement blink and blink once
                break;
            
            default:
                break;
            }

            ESP_ERROR_CHECK(led_strip_refresh(led_strip));
          
        }
    }
    ESP_LOGE(TAG, "LED queue receive failed");
}

void app_main(void) {
    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));

    // create light strip
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));

    // create queue to pass encoder button state to encoder_task
    encoder_btn_evt_queue = xQueueCreate(5, sizeof(bool));
    if ( encoder_btn_evt_queue == 0) {
        ESP_LOGE(TAG, "Queue for encoder button was not created");
        return;
    }
    // create queue to pass LED state events
    led_evt_queue = xQueueCreate(5, sizeof(led_state_t));
    if ( led_evt_queue == 0) {
        ESP_LOGE(TAG, "Queue for LED events was not created");
        return;
    }
    switch_driver_init(button_func_pair, PAIR_SIZE(button_func_pair), esp_zb_buttons_handler);
    xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 4, NULL);
    xTaskCreate(encoder_task, "Ecoder_main", 4096, NULL, 5, NULL);
    xTaskCreate(led_task, "LED_main", 4096, NULL, 6, NULL);


    ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 0, 16, 0, 0));
    ESP_ERROR_CHECK(led_strip_refresh(led_strip));

}
