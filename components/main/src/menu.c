#include <stdint.h>
#include <string.h>

#include "menu.h"
#include "display.h"

#define kMaxPeerDevices (32)
static PeerDeviceData peerDevices[kMaxPeerDevices] = {};
static size_t peerDevicesCount = 0;

static const size_t kMenuStartRow = 0;
static const size_t kMenuEndRow = 4;

static size_t pickedMenuItem = 0;

static const char kPickingArrow[] = "<-";
static const uint8_t kPickingArrowLen = sizeof(kPickingArrow);
static const uint8_t kPickingArrowWidth = 16;

static DisplayDevice *display = NULL;

static void drawMenu();

void setMenuDisplay(DisplayDevice *newDisplay) {
    display = newDisplay;
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
    if (newState == DEVICE_STATE_IDLE) {
        drawMenu();

    } else if (newState == DEVICE_STATE_DISCOVERING) {
        peerDevicesCount = 0;
    }
}

void encoderCallback(EncoderEvent event, void *param) {
    switch (event) {
    case ENCODER_STEP_CW:
        if (pickedMenuItem + 1 < peerDevicesCount) {
            pickedMenuItem++;
            drawMenu();
        }
        break;
    case ENCODER_STEP_CCW:
        if (pickedMenuItem > 0) {
            pickedMenuItem--;
            drawMenu();
        }
        break;
    case ENCODER_SWITCH_PRESSED:
        connectToDevice(&peerDevices[pickedMenuItem]);
        break;
    }

}

static void drawMenu() {
    for (uint8_t row = kMenuStartRow; row < kMenuEndRow; ++row) {
        if (pickedMenuItem + row >= peerDevicesCount) {
            eraseRowPart(display, row, 0, display->width);
            continue;
        }

        drawString(display, peerDevices[pickedMenuItem + row].name, peerDevices[pickedMenuItem + row].nameLen, row, 
                   0, display->width - kPickingArrowWidth, ALIGNMENT_LEFT);
    }

    drawString(display, kPickingArrow, kPickingArrowLen, 0,
               display->width - kPickingArrowWidth, display->width, ALIGNMENT_RIGHT);

    displayBuffer(display);
}
