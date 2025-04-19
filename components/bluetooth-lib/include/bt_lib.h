#ifndef BT_LIB_HPP_
#define BT_LIB_HPP_

#include <esp_bt_defs.h>
#include <esp_gap_bt_api.h>
#include <stdint.h>
#include <esp_a2dp_api.h>
#include <esp_avrc_api.h>
#include <freertos/idf_additions.h>

#include "dispatcher.h"

typedef enum {
    A2DP_STATE_IDLE,
    A2DP_STATE_DISCOVERING,
    A2DP_STATE_DISCOVERED,
    A2DP_STATE_CONNECTING,
    A2DP_STATE_CONNECTED,
    A2DP_STATE_DISCONNECTING,
    A2DP_STATE_DISCONNECTED,
} A2DPState;

typedef enum {
    AUDIO_IDLE,
    AUDIO_STARTED,
} AudioState;

typedef struct __attribute__((packed)) {
    uint16_t channel1;
    uint16_t channel2;
} AudioFrame;

typedef int32_t (*AudioDataCallback)(AudioFrame *, int32_t);

typedef struct {
    A2DPState connectionState;
    AudioState audioState;

    uint8_t peerDeviceName[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
    uint8_t peerDeviceNameLen;

    esp_bd_addr_t peerBda;

    Dispatcher btDispatcher;
    TimerHandle_t heartBeatTimer;

    esp_avrc_rn_evt_cap_mask_t avrcNotificationEventCapabilities;

    AudioDataCallback audioCallback;

    int constructionToken;
} BluetoothDevice;

void initBtDevice(AudioDataCallback dataCallback);

#endif
