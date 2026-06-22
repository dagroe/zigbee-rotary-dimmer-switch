#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_zigbee_core.h"

// Debug flag
// #define DEBUG_ENABLED 0

/* LED config ======================================================================== */
#define LED_GPIO GPIO_NUM_1

/* Relay config ====================================================================== */
/* Drives Q1 (NPN) for the on-board 230V relay K1. GPIO high = relay energized.
   Moved off strapping pin GPIO15 to GPIO2 on the rewired board. */
#define RELAY_GPIO GPIO_NUM_2

/* Button config ===================================================================== */
/* Moved off GPIO9 (a strapping/boot pin): holding it at power-up forced serial
   download mode and the app would not start. GPIO23 is a plain, non-strapping
   GPIO. Wiring is unchanged: button to GND, external pull-up to 3.3V, active-low. */
#define GPIO_INPUT_COMMISSION_SWITCH  GPIO_NUM_23

/* Dedicated button to toggle the local 230V relay (works without a coordinator). */
#define GPIO_INPUT_RELAY_SWITCH       GPIO_NUM_18

/* Encoder config ==================================================================== */
#define GPIO_INPUT_IO_TOGGLE_SWITCH  GPIO_NUM_20

#define ROT_ENC_A_GPIO GPIO_NUM_21
#define ROT_ENC_B_GPIO GPIO_NUM_22

#define ENABLE_HALF_STEPS true  // Set to true to enable tracking of rotary encoder at half step resolution
#define RESET_AT          0      // Set to a positive non-zero number to reset the position if this value is exceeded
#define FLIP_DIRECTION    true   // Set to true to reverse the clockwise/counterclockwise sense

/* External daughterboard ports ====================================================== */
/* Two pin headers brought out so the in-wall module can drive a second control
   surface: an external rotary encoder (a second dimmer) and/or a plain wall
   switch. Both are always exposed as Zigbee endpoints (3 and 4); if nothing is
   wired in, the inputs idle high (internal pull-ups) and simply never fire, so
   the endpoints just stay unbound and unused.

   External encoder port: A, B, push-button, plus GND and 3.3V. */
#define EXT_ROT_ENC_A_GPIO            GPIO_NUM_6
#define EXT_ROT_ENC_B_GPIO            GPIO_NUM_7
#define GPIO_INPUT_EXT_ENCODER_BUTTON GPIO_NUM_19

/* External switch port: a single contact to GND (active-low). */
#define GPIO_INPUT_EXT_SWITCH         GPIO_NUM_14

/* Zigbee configuration ============================================================== */
#define MAX_CHILDREN                    10          /* the max amount of connected devices */
#define INSTALLCODE_POLICY_ENABLE       false       /* false: join with the default TC link key (what zigbee2mqtt/ZHA use by default) */
#define HA_ONOFF_SWITCH_ENDPOINT        1           /* the rotary/button controller endpoint */
#define HA_RELAY_ENDPOINT               2           /* on/off server endpoint for the local 230V relay */
#define HA_EXT_ENCODER_ENDPOINT         3           /* external encoder daughterboard (second dimmer) controller */
#define HA_EXT_SWITCH_ENDPOINT          4           /* external wall switch (simple on/off) controller */
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
        .radio_mode = ZB_RADIO_MODE_NATIVE,                     \
    }

#define ESP_ZB_DEFAULT_HOST_CONFIG()                            \
    {                                                           \
        .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE,   \
    }

void configure_device(void);

#ifdef __cplusplus
} // extern "C"
#endif