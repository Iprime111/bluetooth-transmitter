#ifndef MENU_H_
#define MENU_H_

#include "bt_lib.h"
#include "display.h"
#include "encoder.h"

void encoderCallback(EncoderEvent event, void *param);
void handleDeviceDiscoveredEvent(PeerDeviceData *peer);
void handleDeviceStateChangedEvent(DeviceState newState);

void setMenuDisplay(DisplayDevice *display);

#endif
