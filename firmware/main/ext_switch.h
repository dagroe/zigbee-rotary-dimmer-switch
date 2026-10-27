#pragma once

#include <stdbool.h>
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Behavior of the external switch port (selected by EXT_SWITCH_MODE in
   device_config.h). */
#define EXT_SWITCH_MODE_PUSHBUTTON  0   /* momentary: toggle once per press (on contact close) */
#define EXT_SWITCH_MODE_ROCKER      1   /* latching: toggle on every position change */

/* Called from the ext-switch task (not ISR context) whenever a toggle is due. */
typedef void (*ext_switch_callback_t)(void);

/**
 * @brief Start the debounced external switch input.
 *
 * Must be called after switch_driver_init(), which installs the shared GPIO ISR
 * service (IRAM). The level at boot is taken as the initial position and does
 * not trigger a toggle.
 */
bool ext_switch_init(gpio_num_t pin, int mode, ext_switch_callback_t cb);

#ifdef __cplusplus
} // extern "C"
#endif
