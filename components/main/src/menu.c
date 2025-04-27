#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "menu.h"
#include "bt_lib.h"
#include "display.h"
#include "encoder.h"

static MenuState currentMenuState = MENU_STARTUP;

static PeerDeviceData peerDevices[kMaxPeerDevices] = {};
static size_t peerDevicesCount = 0;

static size_t pickedMenuItem = 0;

static bool isPlayingAudio = false;
static uint8_t volumeLevel = kDefaultAudioLevel;
static bool isFocusedOnAudio = false;

static DisplayDevice *display = NULL;

static void encoderDeviceSelectionMenu(EncoderEvent event);
static void encoderAudioControlMenu(EncoderEvent event);
static void encoderStartupMenu(EncoderEvent event);

static void drawTextMenu(const char *line1, const char *line2, const char *line3, const char *line4);
static void drawDeviceSelectionMenu();
static void drawDiscoveryMenu();
static void drawAudioControlMenu();
static void drawStartupMenu();
static void drawConnectionMenu();
static void drawDisconnectionMenu();

void setMenuDisplay(DisplayDevice *newDisplay) {
    display = newDisplay;
    drawStartupMenu();
}

void volumeChangedCallback(uint8_t newVolumeLevel) {
    volumeLevel = newVolumeLevel;
    drawAudioControlMenu();
}

void handleDeviceDiscoveredEvent(PeerDeviceData *peer) {
    assert(peer);

    if (peerDevicesCount >= kMaxPeerDevices) {
        return;
    }

    for (uint8_t deviceIdx = 0; deviceIdx < peerDevicesCount; deviceIdx++) {
        if (memcmp(peer->address, peerDevices[deviceIdx].address, sizeof(esp_bd_addr_t)) == 0) {
            return;
        }
    }

    peerDevices[peerDevicesCount] = *peer;
    peerDevicesCount++;
}

void handleDeviceStateChangedEvent(DeviceState newState) {
    switch (newState) {

    case DEVICE_STATE_IDLE:
        currentMenuState = MENU_DEVICE_SELECTION;
        drawDeviceSelectionMenu();
        break;
    case DEVICE_STATE_DISCOVERING:
        currentMenuState = MENU_DISCOVERY_IN_PROGRESS;
        peerDevicesCount = 0;
        pickedMenuItem = 0;
        drawDiscoveryMenu();
        break;
    case DEVICE_STATE_CONNECTING:
        currentMenuState = MENU_CONNECTION;
        drawConnectionMenu();
        break;
    case DEVICE_STATE_CONNECTED:
        currentMenuState = MENU_AUDIO_CONTROL;
        pickedMenuItem = 0;
        isPlayingAudio = false;
        isFocusedOnAudio = false;
        setVolume(kDefaultAudioLevel);
        break;
    case DEVICE_STATE_DISCONNECTING:
        currentMenuState = MENU_DISCONNECTION;
        drawDisconnectionMenu();
        break;
    case DEVICE_STATE_DISCONNECTED:
        currentMenuState = MENU_STARTUP;
        drawStartupMenu();
        break;
    }
}

void encoderCallback(EncoderEvent event, void *param) {
    switch (currentMenuState) {
    case MENU_DEVICE_SELECTION:
        encoderDeviceSelectionMenu(event);
        break;
    case MENU_AUDIO_CONTROL:
        encoderAudioControlMenu(event);
        break;
    case MENU_STARTUP:
        encoderStartupMenu(event);
        break;
    case MENU_CONNECTION:
    case MENU_DISCONNECTION:
    case MENU_DISCOVERY_IN_PROGRESS:
        break;
    }
}

static void encoderDeviceSelectionMenu(EncoderEvent event) {
    switch (event) {
    case ENCODER_STEP_CW:
        if (pickedMenuItem + 1 <= peerDevicesCount) {
            pickedMenuItem++;
            drawDeviceSelectionMenu();
        }
        break;
    case ENCODER_STEP_CCW:
        if (pickedMenuItem > 0) {
            pickedMenuItem--;
            drawDeviceSelectionMenu();
        }
        break;
    case ENCODER_SWITCH_PRESSED:
        if (pickedMenuItem < peerDevicesCount) {
            connectToDevice(&peerDevices[pickedMenuItem]);
        } else {
            startDiscovery(kDiscoveryDuration);
        }
        break;
    }
}

static void encoderAudioControlMenu(EncoderEvent event) {
    switch (event) {
    case ENCODER_STEP_CW:
        if (isFocusedOnAudio) {
            setVolume((volumeLevel + kAudioStep) % 101);
        } else if (pickedMenuItem + 1 < kAudioControlMenuEntries) {
            pickedMenuItem++;
            drawAudioControlMenu();
        }
        break;
    case ENCODER_STEP_CCW:
        if (isFocusedOnAudio) {
            if (volumeLevel < kAudioStep) {
                setVolume(0);
            } else {
                setVolume(volumeLevel - kAudioStep);
            }
        }else if (pickedMenuItem > 0) {
            pickedMenuItem--;
            drawAudioControlMenu();
        }
        break;
    case ENCODER_SWITCH_PRESSED:
        switch ((AudioMenuEntries)pickedMenuItem) {
        case AUDIO_MENU_PLAY_BUTTON:
            if (isPlayingAudio) {
                stopAudio();
            } else {
                startAudio();
            }

            isPlayingAudio = !isPlayingAudio;
            drawAudioControlMenu();
            break;
        case AUDIO_MENU_VOLUME:
            isFocusedOnAudio = !isFocusedOnAudio;
            break;
        case AUDIO_MENU_BACK_BUTTON:
            disconnectFromDevice();
            break;
        }
        break;
    }
}

static void encoderStartupMenu(EncoderEvent event) {
    if (event != ENCODER_SWITCH_PRESSED) {
        return;
    }

    startDiscovery(kDiscoveryDuration);
}

static void drawDeviceSelectionMenu() {
    uint8_t textRightBorder = display->width - kPickingArrowWidth;

    for (uint8_t row = kMenuStartRow; row < kMenuEndRow; ++row) {
        if (pickedMenuItem + row == peerDevicesCount) {
            drawString(display, kRestartText, kRestartTextLen, row, 0, textRightBorder, ALIGNMENT_LEFT);
            continue;
        }

        if (pickedMenuItem + row > peerDevicesCount) {
            eraseRowPart(display, row, 0, display->width);
            continue;
        }

        drawString(display, peerDevices[pickedMenuItem + row].name, peerDevices[pickedMenuItem + row].nameLen, row, 
                   0, textRightBorder, ALIGNMENT_LEFT);
    }

    drawString(display, kPickingArrow, kPickingArrowLen, 0,
               textRightBorder, display->width, ALIGNMENT_RIGHT);

    displayBuffer(display);
}

static void drawAudioControlMenu() {
    uint8_t textRightBorder = display->width - kPickingArrowWidth;

    // Text with max possible size
    char text[20] = {};
    uint8_t textLen = 20;

    for (uint8_t row = kMenuStartRow; row < kMenuEndRow; ++row) {
        if (pickedMenuItem + row >= kAudioControlMenuEntries) {
            eraseRowPart(display, row, 0, display->width);
            continue;
        }

        switch ((AudioMenuEntries)(pickedMenuItem + row)) {
        case AUDIO_MENU_PLAY_BUTTON:
            if (isPlayingAudio) {
                textLen = snprintf(text, sizeof(text), "STOP");
            } else {
                textLen = snprintf(text, sizeof(text), "PLAY");
            }
            break;
        case AUDIO_MENU_VOLUME:
            textLen = snprintf(text, sizeof(text), "Volume: %u%%", volumeLevel);
            break;
        case AUDIO_MENU_BACK_BUTTON:
            textLen = snprintf(text, sizeof(text), "BACK");
            break;
        }

        drawString(display, text, textLen, row, 0, textRightBorder, ALIGNMENT_LEFT);
    }

    drawString(display, kPickingArrow, kPickingArrowLen, 0,
               textRightBorder, display->width, ALIGNMENT_RIGHT);

    displayBuffer(display);
}

static void drawTextMenu(const char *line1, const char *line2, const char *line3, const char *line4) {
    drawStringFullLine(display, line1, 0, ALIGNMENT_LEFT);
    drawStringFullLine(display, line2, 1, ALIGNMENT_LEFT);
    drawStringFullLine(display, line3, 2, ALIGNMENT_LEFT);
    drawStringFullLine(display, line4, 3, ALIGNMENT_LEFT);

    displayBuffer(display);
}

static void drawDiscoveryMenu() {
    drawTextMenu("", kDiscoveryText, "", "");
}

static void drawStartupMenu() {
    drawTextMenu("", kStartupText1, kStartupText2, "");
}

static void drawConnectionMenu() {
    drawTextMenu("", kConnectingText, "", "");
}

static void drawDisconnectionMenu() {
    drawTextMenu("", kDisconnectingText, "", "");
}
