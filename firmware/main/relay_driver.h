#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configure the relay GPIO as an output and drive it OFF (de-energized).
 *        Call once, early in app_main, so the 230V relay is in a known safe
 *        state immediately at boot.
 */
void relay_init(void);

/** @brief Energize (true) or release (false) the relay. */
void relay_set(bool on);

/** @brief Current relay state. */
bool relay_get(void);

#ifdef __cplusplus
}
#endif
