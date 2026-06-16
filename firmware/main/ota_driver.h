#pragma once

#include "esp_err.h"
#include "esp_zigbee_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * OTA identity. These three values tie the running firmware to the .ota image
 * a coordinator (zigbee2mqtt, ZHA, deCONZ, ...) offers. The coordinator only
 * pushes an image whose header file_version is GREATER than the running
 * version, and whose manufacturer code + image type match exactly.
 *
 * On every release: bump OTA_UPGRADE_FILE_VERSION here AND build the .ota with
 * the same manufacturer/image-type and a matching (higher) version.
 * See firmware/docs/OTA.md.
 * ------------------------------------------------------------------------- */
#define OTA_UPGRADE_MANUFACTURER   0x131B      /* 16-bit manufacturer code (Espressif default) */
#define OTA_UPGRADE_IMAGE_TYPE     0x1010      /* product image type (manufacturer-specific) */
#define OTA_UPGRADE_FILE_VERSION   0x01000000  /* running firmware version */
#define OTA_UPGRADE_HW_VERSION     0x0101      /* board revision */

/* Max ZCL OTA block payload the client requests (bytes). */
#define OTA_UPGRADE_MAX_DATA_SIZE  223

/* How often (minutes) the client asks the server whether a newer image exists. */
#define OTA_UPGRADE_QUERY_INTERVAL_MIN  60

/**
 * @brief Build the OTA Upgrade (client) cluster and append it to a cluster list.
 *        Call while assembling the endpoint in configure_device().
 */
esp_err_t ota_add_cluster(esp_zb_cluster_list_t *cluster_list);

/**
 * @brief Start the OTA client on @p endpoint: sets the periodic image-query
 *        interval. Call once, after esp_zb_start().
 */
void ota_client_start(uint8_t endpoint);

/**
 * @brief Health check for OTA rollback. Call after the device has rejoined the
 *        network. If the running image is a freshly-OTA'd one awaiting
 *        verification, this confirms it so the bootloader keeps it; otherwise a
 *        crash/wedge before this point rolls back to the previous image. No-op
 *        for normally-booted images.
 */
void ota_confirm_running_image(void);

/**
 * @brief Handle ESP_ZB_CORE_OTA_UPGRADE_VALUE_CB_ID. Writes the downloaded image
 *        to the spare OTA partition and reboots into it when complete.
 */
esp_err_t ota_handle_value(const esp_zb_zcl_ota_upgrade_value_message_t *msg);

#ifdef __cplusplus
}
#endif
