#ifndef MENU_H_
#define MENU_H_

#include "bt_lib.h"
#include "display.h"
#include "encoder.h"

typedef enum {
    MENU_DEVICE_SELECTION,
    MENU_AUDIO_CONTROL,
    MENU_STARTUP,
    MENU_DISCOVERY_IN_PROGRESS,
    MENU_CONNECTION,
    MENU_DISCONNECTION,
} MenuState;

typedef enum {
    AUDIO_MENU_PLAY_BUTTON = 0,
    AUDIO_MENU_VOLUME = 1,
    AUDIO_MENU_BACK_BUTTON = 2,
} AudioMenuEntries;

#define kMaxPeerDevices (32)

#define kMenuStartRow (0)
#define kMenuEndRow (4)

#define kPickingArrow "<-"
#define kPickingArrowLen (sizeof(kPickingArrow) - 1)
#define kPickingArrowWidth (16)

#define kRestartText "RESTART DISCOVERY"
#define kRestartTextLen (sizeof(kRestartText) - 1)

#define kDiscoveryText "SCANNING..."
#define kStartupText1 "PRESS BUTTON"
#define kStartupText2 "TO START"
#define kConnectingText "CONNECTING..."
#define kDisconnectingText "DISCONNECTIG..."

#define kDiscoveryDuration (5)

#define kAudioControlMenuEntries (3)

#define kDefaultAudioLevel (25)
#define kAudioStep (5)

void volumeChangedCallback(uint8_t newVolumeLevel);
void encoderCallback(EncoderEvent event, void *param);
void handleDeviceDiscoveredEvent(PeerDeviceData *peer);
void handleDeviceStateChangedEvent(DeviceState newState);

void setMenuDisplay(DisplayDevice *display);

#endif
