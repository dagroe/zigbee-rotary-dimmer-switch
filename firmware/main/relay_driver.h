#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The on-board relay's NORMALLY-CLOSED (NC) contact carries the 230V load:
 *   - coil DE-energized (GPIO low)  -> NC closed -> outlet POWERED   (default)
 *   - coil energized     (GPIO high) -> NC open  -> outlet CUT
 * So an unpowered/dead device leaves the load powered (the lamp keeps working),
 * and we energize the coil only to actively cut power (e.g. emergency).
 *
 * The driver models state as "outlet powered" -- the same meaning as the Zigbee
 * on/off attribute -- and hides the coil inversion.
 */

/** @brief Configure the relay GPIO and drive the outlet to its default
 *         (powered). Call once, early in app_main. */
void relay_init(void);

/** @brief Power (true) or cut (false) the outlet. */
void relay_set(bool outlet_on);

/** @brief Current outlet-powered state. */
bool relay_get(void);

#ifdef __cplusplus
}
#endif
