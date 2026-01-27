#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_zigbee_core.h"

// Debug flag
//#define DEBUG_ENABLED 0

/* LED config ======================================================================== */
#define LED_GPIO GPIO_NUM_1

/* Button config ===================================================================== */
#define GPIO_INPUT_COMMISSION_SWITCH  GPIO_NUM_9

/* Encoder config ==================================================================== */
#define GPIO_INPUT_IO_TOGGLE_SWITCH  GPIO_NUM_20

#define ROT_ENC_A_GPIO GPIO_NUM_21
#define ROT_ENC_B_GPIO GPIO_NUM_22

#define ENABLE_HALF_STEPS true  // Set to true to enable tracking of rotary encoder at half step resolution
#define RESET_AT          0      // Set to a positive non-zero number to reset the position if this value is exceeded
#define FLIP_DIRECTION    false  // Set to true to reverse the clockwise/counterclockwise sense

/* Zigbee configuration ============================================================== */
#define MAX_CHILDREN                    10          /* the max amount of connected devices */
#define INSTALLCODE_POLICY_ENABLE       true        /* enable the install code policy for security */
#define HA_ONOFF_SWITCH_ENDPOINT        1           /* esp light switch device endpoint */
#define ESP_ZB_PRIMARY_CHANNEL_MASK     ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK  /* Zigbee primary channel mask use in the example */

#define ESP_ZB_ZC_CONFIG()                                                              \
    {                                                                                   \
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ROUTER,                                       \
        .install_code_policy = INSTALLCODE_POLICY_ENABLE,                               \
        .nwk_cfg.zczr_cfg = {                                                           \
            .max_children = MAX_CHILDREN,                                               \
        },                                                                              \
    }

#define ESP_ZB_DEFAULT_RADIO_CONFIG()                           \
    {                                                           \
        .radio_mode = RADIO_MODE_NATIVE,                        \
    }

#define ESP_ZB_DEFAULT_HOST_CONFIG()                            \
    {                                                           \
        .host_connection_mode = HOST_CONNECTION_MODE_NONE,      \
    }

void configure_device(void);

#ifdef __cplusplus
} // extern "C"
#endif