#include "string.h"
#include "freertos/FreeRTOS.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "device_config.h"
#include "ota_driver.h"
#include "version.h"

char modelid[] = { 12, 'E','S','P','_','D','I','M','M','E','R','_','1' };
char manufname[] = { 14, 'D','G',' ','E','l','e','c','t','r','o','n','i','c','s' };
/* ZCL string ([len][chars...]); filled from FW_VERSION_STRING in configure_device. */
char sw_build_version[16];

void configure_device(void)
{
    /* Basic-cluster SW Build ID = the single-source firmware version string. */
    size_t vlen = strlen(FW_VERSION_STRING);
    sw_build_version[0] = (char)vlen;
    memcpy(&sw_build_version[1], FW_VERSION_STRING, vlen);

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
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_SW_BUILD_ID, &sw_build_version[0]);
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
    esp_zb_attribute_list_t *esp_zb_on_off_switch_cluster = esp_zb_on_off_switch_config_cluster_create(&on_off_switch_cfg);

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

    /* OTA Upgrade (client) cluster: lets any standard coordinator push firmware. */
    ESP_ERROR_CHECK(ota_add_cluster(esp_zb_cluster_list));

    esp_zb_ep_list_t *esp_zb_ep_list = esp_zb_ep_list_create();
    /* add created endpoint (cluster_list) to endpoint list */
    esp_zb_endpoint_config_t endpoint_config = {
        .endpoint = HA_ONOFF_SWITCH_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_ON_OFF_SWITCH_DEVICE_ID,
        .app_device_version = 0,
    };
    esp_zb_ep_list_add_ep(esp_zb_ep_list, esp_zb_cluster_list, endpoint_config);
    esp_zb_device_register(esp_zb_ep_list);
}