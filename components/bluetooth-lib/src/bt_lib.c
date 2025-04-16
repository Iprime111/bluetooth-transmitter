#include <assert.h>
#include <esp_system.h>
#include <esp_bt.h>
#include <esp_err.h>
#include <esp_bt_main.h>
#include <esp_bt_device.h>
#include <esp_gap_bt_api.h>
#include <esp_a2dp_api.h>
#include <esp_avrc_api.h>
#include <esp_log.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <stdint.h>
#include <string.h>
#include <esp_bt_defs.h>

#include "bt_lib.h"
#include "dispatcher.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "portmacro.h"
#include "utils.h"

#define BT_DEVICE_TAG "BT_DEV_AUDIO"
// AVRCP used transaction label
#define APP_RC_CT_TL_GET_CAPS            (0)
#define APP_RC_CT_TL_RN_VOLUME_CHANGE    (1)

#define CHECK_CONSTRUCTION_TOKEN()                                                          \
    do {                                                                                    \
        if (device.constructionToken == 0) {                                                \
            ESP_LOGE(BT_DEVICE_TAG, "can't use bt device that has a construction error");   \
            abort();                                                                        \
        }                                                                                   \
    } while (0)

enum {
    HEART_BEAT_EVENT = 0xff00, // Shows state handler that it was called from the heart beat timer
};

static BluetoothDevice device = {
    .connectionState = A2DP_STATE_IDLE,
    .audioState = AUDIO_IDLE,
    .peerDeviceName = {0},
    .peerDeviceNameLen = 0,
    .peerBda = {0},
    .btDispatcher = { NULL, NULL },
    .heartBeatTimer = NULL,
    .constructionToken = 0,
};

static const uint32_t kHeartBeatTimerPeriodMs = 10000;
static const uint8_t kInquiryDuration = 10; // Discovery inquiry duration
static const char kDeviceName[] = "bluetooth-transmitter";

// TODO remove
//static const char kRemoteDeviceName[] = "HUAWEI FreeBuds 5i";
static const char kRemoteDeviceName[] = "JBL Clip 4";

static void launchDevice(uint16_t unused1, void *dataCallback);

static void gapCallback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);

static void filterScanResult(esp_bt_gap_cb_param_t *param);

static void handleDiscoveryStateChanged(esp_bt_gap_cb_param_t *param);
static void handleLegacyPinPairing(esp_bt_gap_cb_param_t *param);

static void a2dpCallback(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param);

static void heartBeatTimer(TimerHandle_t timer);

static void deviceStateHandler(uint16_t event, void *param);

static void connectingStateHandler(uint16_t event, void *param);
static void connectedStateHandler(uint16_t event, void *param);
static void disconnectingStateHandler(uint16_t event, void *param);
static void disconnectedStateHandler(uint16_t event, void *param);

static void processAudioState(uint16_t event, esp_a2d_cb_param_t *param);

static void avrcCallback(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param);
static void handleAVRCEvent(uint16_t event, void *param);
static void avrcVolumeChanged();
static void avrcNotificationEvent(uint8_t event_id, esp_avrc_rn_param_t *event_parameter);

void initBtDevice(esp_a2d_source_data_cb_t dataCallback) {
    assert(dataCallback);

    if (device.constructionToken != 0) {
        ESP_LOGE(BT_DEVICE_TAG, "Can't initialize a bluetooth device twice");
        return;
    }

    // Initialize BluetoothDevice fields
    device.connectionState = A2DP_STATE_IDLE;
    device.audioState = AUDIO_IDLE;
    device.peerDeviceNameLen = 0;
    device.heartBeatTimer = NULL;
    device.constructionToken = 0;

    esp_err_t nvsErr = nvs_flash_init();
    if (nvsErr == ESP_ERR_NVS_NO_FREE_PAGES || nvsErr == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvsErr = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvsErr);

    // Release BLE memory
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    esp_bt_controller_config_t btConfig = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    if (esp_bt_controller_init(&btConfig) != ESP_OK) {
        ESP_LOGE(BT_DEVICE_TAG, "Initialize bluetooth controller failed");
        return;
    }

    if (esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT) != ESP_OK) {
        ESP_LOGE(BT_DEVICE_TAG, "Enable bluetooth controller failed");
        return;
    }

    esp_bluedroid_config_t bluedroidConfig = BT_BLUEDROID_INIT_CONFIG_DEFAULT();

#if (CONFIG_SSP_ENABLED == true)
    bluedroidConfig.ssp_en = true;
#else
    bluedroidConfig.ssp_en = false;
#endif

    if (esp_bluedroid_init_with_cfg(&bluedroidConfig) != ESP_OK) {
        ESP_LOGE(BT_DEVICE_TAG, "Initialize bluedroid failed");
        return;
    }

    if (esp_bluedroid_enable() != ESP_OK) {
        ESP_LOGE(BT_DEVICE_TAG, "Enable bluedroid failed");
        return;
    }

#if (CONFIG_SSP_ENABLED == true)
    // Set default parameters for Secure Simple Pairing
    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_IO;
    esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));
#endif

    // Set default parameters for Legacy Pairing
    // Use variable pin, input pin code when pairing
    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_VARIABLE;
    esp_bt_pin_code_t pin_code;
    esp_bt_gap_set_pin(pin_type, 0, pin_code);

    char deviceBda[18];
    bdaToStr(esp_bt_dev_get_address(), deviceBda, sizeof(deviceBda));
    ESP_LOGI(BT_DEVICE_TAG, "Device address: [%s]", deviceBda);

    device.constructionToken = 1;

    // Init task dispatcher and dispatch connection routine
    initDispatcher(&device.btDispatcher);
    dispatchTask(&device.btDispatcher, launchDevice, 0, &dataCallback, sizeof(dataCallback));
}

static void launchDevice(uint16_t unused1, void *dataCallback) {
    CHECK_CONSTRUCTION_TOKEN();

    // Init generic access profile
    ESP_ERROR_CHECK(esp_bt_gap_set_device_name(kDeviceName));
    ESP_ERROR_CHECK(esp_bt_gap_register_callback(gapCallback));

    // Init audio/video remote control protocol
    ESP_ERROR_CHECK(esp_avrc_ct_init());
    ESP_ERROR_CHECK(esp_avrc_ct_register_callback(avrcCallback));

    // Set used avrc events (volume changes)
    esp_avrc_rn_evt_cap_mask_t eventMask = {};
    esp_avrc_rn_evt_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_SET, &eventMask, ESP_AVRC_RN_VOLUME_CHANGE);
    ESP_ERROR_CHECK(esp_avrc_tg_set_rn_evt_cap(&eventMask));

    ESP_ERROR_CHECK(esp_a2d_source_init());
    esp_a2d_register_callback(a2dpCallback);
    esp_a2d_source_register_data_callback(*((esp_a2d_source_data_cb_t *)dataCallback));

    // Prevent connections from peer devices
    esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
    esp_bt_gap_get_device_name();

    // Start discovery
    ESP_LOGI(BT_DEVICE_TAG, "Starting device discovery...");
    device.connectionState = A2DP_STATE_DISCOVERING;
    ESP_ERROR_CHECK(esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, kInquiryDuration, 0));

    // Start heart beat timer
    int timerId = 0;
    device.heartBeatTimer = xTimerCreate("ConnectionTimer", kHeartBeatTimerPeriodMs / portTICK_PERIOD_MS,
                                         pdTRUE, &timerId, heartBeatTimer);
    xTimerStart(device.heartBeatTimer, portMAX_DELAY);
}

static void gapCallback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
    // Callback function for generic access profile
    assert(param);

    switch (event) {
    // Discovered result
    case ESP_BT_GAP_DISC_RES_EVT:
        if (device.connectionState == A2DP_STATE_DISCOVERING) {
            filterScanResult(param);
        }
        break;

    // Discovery state changed
    case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
        handleDiscoveryStateChanged(param);
        break;

    // Authentication complete
    case ESP_BT_GAP_AUTH_CMPL_EVT:
        if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(BT_DEVICE_TAG, "authentication success: %s", param->auth_cmpl.device_name);
            ESP_LOG_BUFFER_HEX(BT_DEVICE_TAG, param->auth_cmpl.bda, ESP_BD_ADDR_LEN);
        } else {
            ESP_LOGE(BT_DEVICE_TAG, "authentication failed, status: %d", param->auth_cmpl.stat);
        }
        break;

    // Pin code requested
    case ESP_BT_GAP_PIN_REQ_EVT:
        handleLegacyPinPairing(param);
        break;

#if (CONFIG_SSP_ENABLED == true)
    // SSP numeric comparison method
    case ESP_BT_GAP_CFM_REQ_EVT:
        ESP_LOGI(BT_DEVICE_TAG, "User pairing confirmation request. Compare the numeric value: %" PRIu32, param->cfm_req.num_val);
        // TODO show number to user
        esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
        break;

    // SSP passkey method 
    // https://community.silabs.com/s/article/bluetooth-pairing-machanism-legacy-pairing-and-secure-simple-pairing-ssp-x?language=en_US
    case ESP_BT_GAP_KEY_NOTIF_EVT:
        ESP_LOGI(BT_DEVICE_TAG, "Pairing passkey notification: %" PRIu32, param->key_notif.passkey);
        break;

    // Also SSP passkey method
    case ESP_BT_GAP_KEY_REQ_EVT:
        ESP_LOGI(BT_DEVICE_TAG, "Please, enter passkey");
        // TODO reply passkey
        break;
#endif

    // GAP mode changed
    case ESP_BT_GAP_MODE_CHG_EVT:
        ESP_LOGI(BT_DEVICE_TAG, "GAP mode changed: %d", param->mode_chg.mode);
        break;

    // Get deice name
    case ESP_BT_GAP_GET_DEV_NAME_CMPL_EVT:
        if (param->get_dev_name_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGW(BT_DEVICE_TAG, "Get device name complete event failed. Status: %d", param->get_dev_name_cmpl.status);
            break;
        }
        
        ESP_LOGI(BT_DEVICE_TAG, "Device name: %s", param->get_dev_name_cmpl.name);
        break;

    // Not used
    default:
        ESP_LOGI(BT_DEVICE_TAG, "GAP event %d will not be handled", event);
        break;
    };
}

static void handleDiscoveryStateChanged(esp_bt_gap_cb_param_t *param) {
    switch (param->disc_st_chg.state) {
    case ESP_BT_GAP_DISCOVERY_STOPPED:
        if (device.connectionState == A2DP_STATE_DISCOVERED) {
            ESP_LOGI(BT_DEVICE_TAG, "Discovery stopped. Connecting to peer %s", device.peerDeviceName);
            device.connectionState = A2DP_STATE_CONNECTING;
            esp_a2d_source_connect(device.peerBda);
        } else {
            // TODO exit to menu
            ESP_LOGW(BT_DEVICE_TAG, "Discovery failed. Restarting...");
            esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, kInquiryDuration, 0);
            
        }
        break;

    case ESP_BT_GAP_DISCOVERY_STARTED:
        // TODO update status?
        ESP_LOGI(BT_DEVICE_TAG, "Discovery started");
      break;
    }
}

static void handleLegacyPinPairing(esp_bt_gap_cb_param_t *param) {
    // TODO user pin code?

    ESP_LOGI(BT_DEVICE_TAG, "Pin code request min_16_digit: %d", param->pin_req.min_16_digit);
    if (param->pin_req.min_16_digit) {
        ESP_LOGI(BT_DEVICE_TAG, "Input pin code: 0000 0000 0000 0000");
        esp_bt_pin_code_t pin_code = {0};
        esp_bt_gap_pin_reply(param->pin_req.bda, true, 16, pin_code);
    } else {
        ESP_LOGI(BT_DEVICE_TAG, "Input pin code: 0000");
        esp_bt_pin_code_t pin_code;
        pin_code[0] = '0';
        pin_code[1] = '0';
        pin_code[2] = '0';
        pin_code[3] = '0';
        esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin_code);
    }
}

static void filterScanResult(esp_bt_gap_cb_param_t *param) {
    assert(param);

    char bdaStr[18];
    ESP_LOGI(BT_DEVICE_TAG, "Scanned device: %s", bdaToStr(param->disc_res.bda, bdaStr, sizeof(bdaStr)));


    uint32_t deviceClass = 0;
    int32_t rssi = -129;
    uint8_t *extInquiryResponse = NULL;


    for (int propertyIdx = 0; propertyIdx < param->disc_res.num_prop; propertyIdx++) {
        esp_bt_gap_dev_prop_t *property = &param->disc_res.prop[propertyIdx];

        switch (property->type) {
        // Class of device
        case ESP_BT_GAP_DEV_PROP_COD:
            deviceClass = *(uint32_t *)(property->val);
            ESP_LOGI(BT_DEVICE_TAG, "\tClass of device: 0x%" PRId32, deviceClass);
            break;

        // Received signal strength indication
        case ESP_BT_GAP_DEV_PROP_RSSI:
            rssi = *(int8_t *)(property->val);
            ESP_LOGI(BT_DEVICE_TAG, "\tRSSI: %" PRId32, rssi);
            break;

        case ESP_BT_GAP_DEV_PROP_EIR:
            extInquiryResponse = (uint8_t *)(property->val);
            break;

        default:
          break;
        }
    }

    // Check if device has "rendering" service class
    if (!esp_bt_gap_is_valid_cod(deviceClass) ||
            !(esp_bt_gap_get_cod_srvc(deviceClass) & ESP_BT_COD_SRVC_RENDERING)) {
        return;
    }

    if (extInquiryResponse) {
        if (!getNameFromEir(extInquiryResponse, device.peerDeviceName, &device.peerDeviceNameLen)) {
            ESP_LOGE(BT_DEVICE_TAG, "Unable to get device name from extended inquiry response");
            return;
        }

        // TODO add devices to queue and show to user
        if (strcmp((char *)device.peerDeviceName, kRemoteDeviceName) == 0) {
            ESP_LOGI(BT_DEVICE_TAG, "Target device found. Address: %s. Name: %s", bdaStr, device.peerDeviceName);
            device.connectionState = A2DP_STATE_DISCOVERED;
            memcpy(device.peerBda, param->disc_res.bda, ESP_BD_ADDR_LEN);
            
            ESP_LOGI(BT_DEVICE_TAG, "Stopping device discovery...");
            esp_bt_gap_cancel_discovery();
        }
    }
}

static void a2dpCallback(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param) {
    dispatchTask(&device.btDispatcher, deviceStateHandler, event, param, sizeof(esp_a2d_cb_param_t));
}

static void heartBeatTimer(TimerHandle_t timer) {
    
    dispatchTask(&device.btDispatcher, deviceStateHandler, HEART_BEAT_EVENT, NULL, 0);
}

static void deviceStateHandler(uint16_t event, void *param) {
    switch (device.connectionState) {

    case A2DP_STATE_CONNECTING:
        connectingStateHandler(event, param);
        break;

    case A2DP_STATE_CONNECTED:
        connectedStateHandler(event, param);
        break;

    case A2DP_STATE_DISCONNECTING:
        disconnectingStateHandler(event, param);
        break;

    case A2DP_STATE_DISCONNECTED:
        disconnectedStateHandler(event, param);
        break;

    case A2DP_STATE_DISCOVERING:
    case A2DP_STATE_DISCOVERED:
        break;

    case A2DP_STATE_IDLE:
        ESP_LOGE(BT_DEVICE_TAG, "Invalid bluetooth device state: IDLE");
        break;

    default:
        ESP_LOGE(BT_DEVICE_TAG, "Invalid bluetooth device state: %d", device.connectionState);
        break;
    }
}

static void connectingStateHandler(uint16_t event, void *param) {
    esp_a2d_cb_param_t *paramPtr = param;

    switch (event) {
    case ESP_A2D_CONNECTION_STATE_EVT:
        if (paramPtr->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
            ESP_LOGI(BT_DEVICE_TAG, "A2DP connected");
            device.connectionState = A2DP_STATE_CONNECTED;
            device.audioState = AUDIO_IDLE;
        } else if (paramPtr->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
            ESP_LOGI(BT_DEVICE_TAG, "A2DP disconnected");
            device.connectionState = A2DP_STATE_DISCONNECTED;
        }
        break;

    case ESP_A2D_AUDIO_STATE_EVT:
    case ESP_A2D_AUDIO_CFG_EVT:
    case ESP_A2D_MEDIA_CTRL_ACK_EVT:
        break;

    case ESP_A2D_REPORT_SNK_DELAY_VALUE_EVT:
        ESP_LOGI(BT_DEVICE_TAG, "Delay value: %u * 1/10 ms", paramPtr->a2d_report_delay_value_stat.delay_value);
        break;

    default:
        ESP_LOGE(BT_DEVICE_TAG, "Unknown A2DP event in the \"connecting\" state: %" PRIu16, event);
        break;

    }
}

static void connectedStateHandler(uint16_t event, void *param) {
    esp_a2d_cb_param_t *paramPtr = param;
    
    switch (event) {
    case ESP_A2D_CONNECTION_STATE_EVT:
        if (paramPtr->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
            ESP_LOGI(BT_DEVICE_TAG, "A2DP disconnected");
            device.connectionState = A2DP_STATE_DISCONNECTED;
        }
        break;

    case ESP_A2D_AUDIO_STATE_EVT:
        break;

    case ESP_A2D_MEDIA_CTRL_ACK_EVT:
    case HEART_BEAT_EVENT:
        processAudioState(event, param);
        break;

    case ESP_A2D_REPORT_SNK_DELAY_VALUE_EVT:
        ESP_LOGI(BT_DEVICE_TAG, "Delay value: %u * 1/10 ms", paramPtr->a2d_report_delay_value_stat.delay_value);
        break;

    default:
        ESP_LOGE(BT_DEVICE_TAG, "Unknown A2DP event in the \"connected\" state: %" PRIu16, event);
        break;
    }
}
 
static void disconnectingStateHandler(uint16_t event, void *param) {
    esp_a2d_cb_param_t *paramPtr = param;

    switch (event) {
    case ESP_A2D_CONNECTION_STATE_EVT:
        if (paramPtr->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
            ESP_LOGI(BT_DEVICE_TAG, "A2DP disconnected");
            device.connectionState = A2DP_STATE_DISCONNECTED;
        }
        break;
    case ESP_A2D_AUDIO_STATE_EVT:
    case ESP_A2D_AUDIO_CFG_EVT:
    case ESP_A2D_MEDIA_CTRL_ACK_EVT:
    case HEART_BEAT_EVENT:
        break;

    case ESP_A2D_REPORT_SNK_DELAY_VALUE_EVT:
        ESP_LOGI(BT_DEVICE_TAG, "Delay value: 0x%u * 1/10 ms", paramPtr->a2d_report_delay_value_stat.delay_value);
        break;

    default: 
        ESP_LOGE(BT_DEVICE_TAG, "Unknown event in the \"disconnecting\" state: %" PRIu16, event);
        break;
    }

}

static void disconnectedStateHandler(uint16_t event, void *param) {
    esp_a2d_cb_param_t *paramPtr = param;

    switch (event) {
    case ESP_A2D_CONNECTION_STATE_EVT:
    case ESP_A2D_AUDIO_STATE_EVT:
    case ESP_A2D_AUDIO_CFG_EVT:
    case ESP_A2D_MEDIA_CTRL_ACK_EVT:
        break;

    case HEART_BEAT_EVENT: {
        uint8_t *bda = device.peerBda;
        ESP_LOGI(BT_DEVICE_TAG, "A2DP connecting to peer: %02x:%02x:%02x:%02x:%02x:%02x",
                 bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
        esp_a2d_source_connect(device.peerBda);
        device.connectionState = A2DP_STATE_CONNECTING;
        break;
    }

    case ESP_A2D_REPORT_SNK_DELAY_VALUE_EVT:
        ESP_LOGI(BT_DEVICE_TAG, "Delay value: %u * 1/10 ms", paramPtr->a2d_report_delay_value_stat.delay_value);
        break;

    default:
        ESP_LOGE(BT_DEVICE_TAG, "Unknown event in the \"disconnected\" state %" PRIu16, event);
        break;
    }
}

static void processAudioState(uint16_t event, esp_a2d_cb_param_t *param) {
    switch (device.audioState) {
    case AUDIO_IDLE:
        if (event == HEART_BEAT_EVENT) {
            ESP_LOGI(BT_DEVICE_TAG, "Checking A2DP");
            esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY);
        } else if (event == ESP_A2D_MEDIA_CTRL_ACK_EVT) {
            if (param && param->media_ctrl_stat.status == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS &&
                param->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY) {

                ESP_LOGI(BT_DEVICE_TAG, "A2DP checked. Starting media...");
                esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
                device.audioState = AUDIO_STARTED;
            }
        }
        break;

    case AUDIO_STARTED:
      break;
    }
}

static void avrcCallback(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param) {
    // Callback function for audio/video remote control protocol
    
    switch (event) {
    case ESP_AVRC_CT_CONNECTION_STATE_EVT:
    case ESP_AVRC_CT_PASSTHROUGH_RSP_EVT:
    case ESP_AVRC_CT_METADATA_RSP_EVT:
    case ESP_AVRC_CT_CHANGE_NOTIFY_EVT:
    case ESP_AVRC_CT_REMOTE_FEATURES_EVT:
    case ESP_AVRC_CT_GET_RN_CAPABILITIES_RSP_EVT:
    case ESP_AVRC_CT_SET_ABSOLUTE_VOLUME_RSP_EVT:
        dispatchTask(&device.btDispatcher, handleAVRCEvent, event, param, sizeof(esp_avrc_ct_cb_param_t));
        break;

    default:
        ESP_LOGE(BT_DEVICE_TAG, "Invalid AVRC event: %d", event);
        break;
    }
}

static void handleAVRCEvent(uint16_t event, void *param) {
    ESP_LOGD(BT_DEVICE_TAG, "Got AVRCP event: %d", event);
    esp_avrc_ct_cb_param_t *paramPtr = (esp_avrc_ct_cb_param_t *)(param);

    switch (event) {
    // Connection state changed
    case ESP_AVRC_CT_CONNECTION_STATE_EVT: {
        uint8_t *bda = paramPtr->conn_stat.remote_bda;
        ESP_LOGI(BT_DEVICE_TAG, "AVRC connection state event: state %d, [%02x:%02x:%02x:%02x:%02x:%02x]",
                 paramPtr->conn_stat.connected, bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);

        if (paramPtr->conn_stat.connected) {
            esp_avrc_ct_send_get_rn_capabilities_cmd(APP_RC_CT_TL_GET_CAPS);
        } else {
            device.avrcNotificationEventCapabilities.bits = 0;
        }
        break;
    }
    // TODO Passthrough responded
    case ESP_AVRC_CT_PASSTHROUGH_RSP_EVT:
        ESP_LOGI(BT_DEVICE_TAG, "AVRC passthrough response: key_code 0x%x, key_state %d, rsp_code %d", paramPtr->psth_rsp.key_code,
            paramPtr->psth_rsp.key_state, paramPtr->psth_rsp.rsp_code);
        break;

    // Metadata responded
    case ESP_AVRC_CT_METADATA_RSP_EVT:
        ESP_LOGI(BT_DEVICE_TAG, "AVRC metadata response: attribute id 0x%x, %s", paramPtr->meta_rsp.attr_id, paramPtr->meta_rsp.attr_text);
        free(paramPtr->meta_rsp.attr_text);
        break;

    // Notification changed
    case ESP_AVRC_CT_CHANGE_NOTIFY_EVT:
        ESP_LOGI(BT_DEVICE_TAG, "AVRC event notification: %d", paramPtr->change_ntf.event_id);
        avrcNotificationEvent(paramPtr->change_ntf.event_id, &paramPtr->change_ntf.event_parameter);
        break;

    // Indicate feature of remote device
    case ESP_AVRC_CT_REMOTE_FEATURES_EVT:
        ESP_LOGI(BT_DEVICE_TAG, "AVRC remote features %"PRIx32", TG features %x", paramPtr->rmt_feats.feat_mask, paramPtr->rmt_feats.tg_feat_flag);
        break;

    // Get supported notification events capability of peer device
    case ESP_AVRC_CT_GET_RN_CAPABILITIES_RSP_EVT:
        ESP_LOGI(BT_DEVICE_TAG, "Remote rn_cap: count %d, bitmask 0x%x", paramPtr->get_rn_caps_rsp.cap_count,
                 paramPtr->get_rn_caps_rsp.evt_set.bits);

        device.avrcNotificationEventCapabilities.bits = paramPtr->get_rn_caps_rsp.evt_set.bits;

        avrcVolumeChanged();
        break;

    // Set absolute volume responded
    case ESP_AVRC_CT_SET_ABSOLUTE_VOLUME_RSP_EVT:
        ESP_LOGI(BT_DEVICE_TAG, "Set absolute volume response: volume %d", paramPtr->set_volume_rsp.volume);
        break;

    default:
        ESP_LOGE(BT_DEVICE_TAG, "Unable to handle AVRCP event: %d", event);
        break;
    }
}

// TODO what's that?
static void avrcVolumeChanged()
{
    if (esp_avrc_rn_evt_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_TEST, &device.avrcNotificationEventCapabilities,
                                           ESP_AVRC_RN_VOLUME_CHANGE)) {
        esp_avrc_ct_send_register_notification_cmd(APP_RC_CT_TL_RN_VOLUME_CHANGE, ESP_AVRC_RN_VOLUME_CHANGE, 0);
    }
}

static void avrcNotificationEvent(uint8_t event_id, esp_avrc_rn_param_t *event_parameter)
{
    switch (event_id) {
    // Volume changed locally on target
    case ESP_AVRC_RN_VOLUME_CHANGE:
        ESP_LOGI(BT_DEVICE_TAG, "Volume changed: %d", event_parameter->volume);
        ESP_LOGI(BT_DEVICE_TAG, "Set absolute volume: volume %d", event_parameter->volume);
        esp_avrc_ct_send_set_absolute_volume_cmd(APP_RC_CT_TL_RN_VOLUME_CHANGE, event_parameter->volume);
        avrcVolumeChanged();
        break;
 
    default:
        break;
    }
}
