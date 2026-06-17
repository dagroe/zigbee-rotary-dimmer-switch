#include "ota_driver.h"

#include <string.h>
#include <inttypes.h>

#include "esp_log.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"

#include "zcl/esp_zigbee_zcl_ota.h"

static const char *TAG = "OTA";

/* An OTA image is a Zigbee OTA file: a header (parsed by the stack and handed to
 * us in msg->ota_header) followed by sub-elements. The actual firmware lives in
 * the first sub-element, prefixed by a 2-byte tag id + 4-byte length. We strip
 * that 6-byte sub-element header from the first received block before writing. */
#define OTA_ELEMENT_HEADER_LEN  6

static const esp_partition_t *s_ota_partition;
static esp_ota_handle_t       s_ota_handle;
static bool                   s_tagid_received;
static uint32_t               s_image_remaining;
static uint32_t               s_written;

static void ota_reboot_cb(uint8_t param)
{
    (void)param;
    ESP_LOGW(TAG, "Rebooting into updated firmware");
    esp_restart();
}

/* Strip the sub-element header from the first block; return the firmware bytes
 * to write in [out, out_len). */
static esp_err_t ota_strip_element(uint32_t image_size, const uint8_t *payload,
                                   uint16_t payload_size, const uint8_t **out, uint16_t *out_len)
{
    if (!s_tagid_received) {
        if (payload_size < OTA_ELEMENT_HEADER_LEN) {
            ESP_LOGE(TAG, "First OTA block too small (%u)", payload_size);
            return ESP_ERR_INVALID_SIZE;
        }
        uint32_t element_len;
        memcpy(&element_len, payload + sizeof(uint16_t), sizeof(element_len)); /* skip 2-byte tag id */
        if (element_len + OTA_ELEMENT_HEADER_LEN != image_size) {
            ESP_LOGE(TAG, "OTA element length %" PRIu32 " + header != image %" PRIu32,
                     element_len, image_size);
            return ESP_ERR_INVALID_SIZE;
        }
        s_tagid_received  = true;
        s_image_remaining = element_len;
        *out     = payload + OTA_ELEMENT_HEADER_LEN;
        *out_len = payload_size - OTA_ELEMENT_HEADER_LEN;
    } else {
        *out     = payload;
        *out_len = payload_size;
    }
    if (*out_len > s_image_remaining) {
        ESP_LOGE(TAG, "OTA overflow: %u > %" PRIu32 " remaining", *out_len, s_image_remaining);
        return ESP_ERR_INVALID_SIZE;
    }
    s_image_remaining -= *out_len;
    return ESP_OK;
}

esp_err_t ota_add_cluster(esp_zb_cluster_list_t *cluster_list)
{
    esp_zb_ota_cluster_cfg_t ota_cfg = {
        .ota_upgrade_file_version        = OTA_UPGRADE_FILE_VERSION,
        .ota_upgrade_downloaded_file_ver = OTA_UPGRADE_FILE_VERSION,
        .ota_upgrade_manufacturer        = OTA_UPGRADE_MANUFACTURER,
        .ota_upgrade_image_type          = OTA_UPGRADE_IMAGE_TYPE,
        .ota_upgrade_file_offset         = ESP_ZB_ZCL_OTA_UPGRADE_FILE_OFFSET_DEF_VALUE,
        .ota_min_block_reque             = ESP_ZB_OTA_UPGRADE_MIN_BLOCK_PERIOD_DEF_VALUE,
        .ota_image_upgrade_status        = ESP_ZB_ZCL_OTA_UPGRADE_IMAGE_STATUS_DEF_VALUE,
    };
    memset(ota_cfg.ota_upgrade_server_id, 0xff, sizeof(ota_cfg.ota_upgrade_server_id));

    esp_zb_attribute_list_t *ota_attr = esp_zb_ota_cluster_create(&ota_cfg);
    if (!ota_attr) {
        ESP_LOGE(TAG, "Failed to create OTA cluster");
        return ESP_FAIL;
    }

    esp_zb_zcl_ota_upgrade_client_variable_t client_var = {
        .timer_query   = ESP_ZB_ZCL_OTA_UPGRADE_QUERY_TIMER_COUNT_DEF,
        .hw_version    = OTA_UPGRADE_HW_VERSION,
        .max_data_size = OTA_UPGRADE_MAX_DATA_SIZE,
    };
    esp_err_t err = esp_zb_ota_cluster_add_attr(ota_attr, ESP_ZB_ZCL_ATTR_OTA_UPGRADE_CLIENT_DATA_ID,
                                                &client_var);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add OTA client data attr: %s", esp_err_to_name(err));
        return err;
    }
    return esp_zb_cluster_list_add_ota_cluster(cluster_list, ota_attr, ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);
}

void ota_client_start(uint8_t endpoint)
{
    esp_zb_ota_upgrade_client_query_interval_set(endpoint, OTA_UPGRADE_QUERY_INTERVAL_MIN);
    ESP_LOGI(TAG, "OTA client started on ep %u, querying every %d min (running v0x%08x)",
             endpoint, OTA_UPGRADE_QUERY_INTERVAL_MIN, (unsigned)OTA_UPGRADE_FILE_VERSION);
}

void ota_confirm_running_image(void)
{
    /* Only a freshly OTA'd image boots in PENDING_VERIFY; for any other image
     * (normal boot, UART flash) this is a no-op. Reaching here means the device
     * rejoined the network, i.e. the new firmware's stack/radio work -- our
     * health check -- so cancel the pending rollback and keep the new image. If
     * the new image had instead crashed/wedged before this point, the watchdog
     * reboot would let the bootloader revert to the previous working image. */
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(running, &state) == ESP_OK &&
        state == ESP_OTA_IMG_PENDING_VERIFY) {
        esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
        if (err == ESP_OK) {
            ESP_LOGW(TAG, "New OTA image confirmed healthy (rejoined network); rollback cancelled");
        } else {
            ESP_LOGE(TAG, "Failed to confirm OTA image: %s", esp_err_to_name(err));
        }
    }
}

esp_err_t ota_handle_value(const esp_zb_zcl_ota_upgrade_value_message_t *msg)
{
    esp_err_t ret = ESP_OK;

    switch (msg->upgrade_status) {
    case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_START:
        s_tagid_received  = false;
        s_image_remaining = 0;
        s_written         = 0;
        s_ota_partition = esp_ota_get_next_update_partition(NULL);
        if (!s_ota_partition) {
            ESP_LOGE(TAG, "No OTA partition available");
            return ESP_FAIL;
        }
        ret = esp_ota_begin(s_ota_partition, OTA_WITH_SEQUENTIAL_WRITES, &s_ota_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(ret));
            return ret;
        }
        ESP_LOGI(TAG, "OTA download started -> partition '%s'", s_ota_partition->label);
        break;

    case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_RECEIVE: {
        const uint8_t *data = NULL;
        uint16_t len = 0;
        ret = ota_strip_element(msg->ota_header.image_size, msg->payload, msg->payload_size, &data, &len);
        if (ret == ESP_OK && len > 0) {
            ret = esp_ota_write(s_ota_handle, data, len);
            if (ret == ESP_OK) {
                s_written += len;
            } else {
                ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(ret));
            }
        }
        break;
    }

    case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_APPLY:
    case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_CHECK:
        /* Stack-driven steps; nothing for the application to do here. */
        break;

    case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_FINISH:
        ret = esp_ota_end(s_ota_handle);
        s_ota_handle = 0;
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_end failed (image rejected): %s", esp_err_to_name(ret));
            break;
        }
        ret = esp_ota_set_boot_partition(s_ota_partition);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "set_boot_partition failed: %s", esp_err_to_name(ret));
            break;
        }
        ESP_LOGW(TAG, "OTA complete (%" PRIu32 " bytes). Rebooting in 2s.", s_written);
        /* Reboot from the stack thread, after the OTA end-response is sent. */
        esp_zb_scheduler_alarm(ota_reboot_cb, 0, 2000);
        break;

    case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_ABORT:
        ESP_LOGW(TAG, "OTA aborted by server/stack");
        if (s_ota_handle) {
            esp_ota_abort(s_ota_handle);
            s_ota_handle = 0;
        }
        s_tagid_received = false;
        break;

    default:
        break;
    }
    return ret;
}
