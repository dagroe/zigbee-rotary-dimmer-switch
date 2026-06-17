#pragma once

/* ---------------------------------------------------------------------------
 * Single source of truth for the firmware version. Bump these three numbers on
 * every release -- nothing else needs editing. They feed:
 *   - FW_VERSION_OTA    : the 32-bit Zigbee OTA file version (ota_driver.h and
 *                         the .ota image header). A coordinator only offers an
 *                         image whose version is greater than the running one.
 *   - FW_VERSION_STRING : the Basic-cluster SW Build ID string z2m displays.
 *   - PROJECT_VER       : the ESP-IDF app version (set from this file in the
 *                         top-level CMakeLists.txt).
 * tools/make_ota.py also reads this file, so the packaged .ota always matches
 * the firmware without passing the version by hand.
 * ------------------------------------------------------------------------- */
#define FW_VERSION_MAJOR 1
#define FW_VERSION_MINOR 0
#define FW_VERSION_PATCH 3

/* 32-bit OTA file version. Monotonic as long as the tuple increases; z2m
 * compares this number to decide whether an offered image is newer.
 * e.g. 1.0.3 -> 0x01000003. */
#define FW_VERSION_OTA \
    (((FW_VERSION_MAJOR) << 24) | ((FW_VERSION_MINOR) << 16) | (FW_VERSION_PATCH))

#define FW__STR(x)  #x
#define FW__XSTR(x) FW__STR(x)
#define FW_VERSION_STRING \
    FW__XSTR(FW_VERSION_MAJOR) "." FW__XSTR(FW_VERSION_MINOR) "." FW__XSTR(FW_VERSION_PATCH)
