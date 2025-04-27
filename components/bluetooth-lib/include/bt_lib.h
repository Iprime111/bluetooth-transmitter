#ifndef BT_LIB_HPP_
#define BT_LIB_HPP_

#include <esp_bt_defs.h>
#include <esp_gap_bt_api.h>
#include <stdint.h>
#include <esp_a2dp_api.h>
#include <esp_avrc_api.h>
#include <freertos/idf_additions.h>

#include "dispatcher.h"

typedef enum : uint16_t {
    DEVICE_STATE_IDLE,
    DEVICE_STATE_DISCOVERING,
    DEVICE_STATE_CONNECTING,
    DEVICE_STATE_CONNECTED,
    DEVICE_STATE_DISCONNECTING,
    DEVICE_STATE_DISCONNECTED,
} DeviceState;

typedef enum {
    AUDIO_STATE_IDLE,
    AUDIO_STATE_STARTING,
    AUDIO_STATE_STARTED,
    AUDIO_STATE_STOPPING,
} AudioState;

typedef struct __attribute__((packed)) {
    uint16_t channel1;
    uint16_t channel2;
} AudioFrame;

typedef struct {
    char name[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
    uint8_t nameLen;

    esp_bd_addr_t address;
} PeerDeviceData;

typedef int32_t (*AudioDataCallback)(AudioFrame *, int32_t);
typedef void (*DeviceStateChangeCallback)(DeviceState);
typedef void (*AudioStateChangedCallback)(AudioState);
typedef void (*DeviceDiscoveredCallback)(PeerDeviceData *);
typedef void (*VolumeChangedCallback)(uint8_t);

typedef enum {
    DEVICE_EVENT_STATE_CHANGED,
    DEVICE_AUDIO_STATE_CHANGED,
    DEVICE_DISCOVERED,
    VOLUME_CHANGED,
} BluetoothDeviceEventType;

typedef struct {
    AudioDataCallback audioDataCallback;
    DeviceStateChangeCallback deviceStateChangedCallback;
    AudioStateChangedCallback audioStateChangedCallback;
    DeviceDiscoveredCallback deviceDiscoveredCallback;
    VolumeChangedCallback volumeChangedCallback;
} BluetoothDeviceCallbacks;

typedef struct {
    DeviceState deviceState;
    AudioState audioState;

    PeerDeviceData selectedPeer;

    Dispatcher btDispatcher;
    TimerHandle_t heartBeatTimer;

    esp_avrc_rn_evt_cap_mask_t avrcNotificationEventCapabilities;

    Dispatcher eventDispatcher;
    BluetoothDeviceCallbacks callbacks;

    int constructionToken;
} BluetoothDevice;

#define kHeartBeatTimerPeriodMs (10000) // Heart beat timer period
#define kDeviceName "bluetooth-transmitter" // Bluetooth device public name

void initBtDevice(BluetoothDeviceCallbacks *callbacks);
bool startAudio();
bool stopAudio();
bool connectToDevice(PeerDeviceData *peer);
bool disconnectFromDevice();
bool startDiscovery(uint8_t inquiryDuration);

bool setVolume(uint8_t volumeLevel);
#endif
