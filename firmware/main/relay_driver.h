#pragma once

#include <stdbool.h>
#include <stdint.h>

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
 *
 * Power-on behavior follows the ZCL StartUpOnOff attribute (z2m
 * "power_on_behavior"): off / on / toggle-previous / previous, persisted in NVS.
 */

/** @brief Configure the relay GPIO and drive the outlet to a safe default
 *         (powered). Call FIRST in app_main, before NVS is available. */
void relay_init(void);

/** @brief Load the persisted power-on behavior + last state from NVS and apply
 *         the resulting outlet state. Call once, after nvs_flash_init(). */
void relay_apply_startup(void);

/** @brief Power (true) or cut (false) the outlet. Does not persist. */
void relay_set(bool outlet_on);

/** @brief Persist the current outlet state as the "last" value (used by the
 *         'previous' power-on behavior). Call after a runtime change. */
void relay_remember(void);

/** @brief Current outlet-powered state. */
bool relay_get(void);

/** @brief StartUpOnOff value (0=off, 1=on, 2=toggle, 0xFF=previous) for the
 *         on/off cluster attribute. */
uint8_t relay_get_startup_behavior(void);

/** @brief Set and persist the StartUpOnOff power-on behavior. */
void relay_set_startup_behavior(uint8_t mode);

#ifdef __cplusplus
}
#endif
