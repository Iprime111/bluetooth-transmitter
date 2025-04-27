#include <assert.h>
#include <esp_log.h>
#include <string.h>

#include "display.h"

static void displayInitSequence(DisplayDevice *display);

void initDisplay(DisplayDevice *display, I2CDevice *i2cDevice, DisplayType type) {
    assert(display);
    assert(i2cDevice);

    display->device = *i2cDevice;
    
    display->type = type;

    display->width = 128;
    display->height = (type == DISPLAY_128_64 ? 64 : 32);


    size_t bufferSize = display->width * display->height / 8;

    display->dataControlByte = (uint8_t *) calloc(bufferSize + 1, sizeof(uint8_t));
    display->buffer = display->dataControlByte + 1;

    *display->dataControlByte = kDataControlByte;

    displayInitSequence(display);

    clearBuffer(display); // Calloc should clear the buffer, but it behaves strange in esp32 libc implementation
    displayBuffer(display);
}

static void displayInitSequence(DisplayDevice *display) {
    uint8_t init1[] = {
        DISPLAY_DISPLAYOFF,         // 0xAE
        DISPLAY_SETDISPLAYCLOCKDIV, // 0xD5
        0x80,                       // the suggested ratio 0x80
        DISPLAY_SETMULTIPLEX        // 0xA8
    }; 
                                       
    sendCommandList(display, init1, sizeof(init1) / sizeof(init1[0]));
    sendSingleCommand(display, display->height - 1);
    
    uint8_t init2[] = {
        DISPLAY_SETDISPLAYOFFSET,   // 0xD3
        0x0,                        // no offset
        DISPLAY_SETSTARTLINE | 0x0, // line #0
        DISPLAY_CHARGEPUMP          // 0x8D
    };        

    sendCommandList(display, init2, sizeof(init2) / sizeof(init2[0]));
    sendSingleCommand(display, 0x14);
    
    uint8_t init3[] = {
        DISPLAY_MEMORYMODE, // 0x20
        0x00, // 0x0 act like ks0108
        DISPLAY_SEGREMAP | 0x1,
        DISPLAY_COMSCANDEC
    };

    sendCommandList(display, init3, sizeof(init3) / sizeof(init3[0]));
    
    uint8_t comPins = 0x02;
    display->contrast = 0x8F;
    
    if (display->type == DISPLAY_128_32) {
        comPins = 0x02;
        display->contrast = 0x8F;
    } else if (display->type == DISPLAY_128_64) {
        comPins = 0x12;
        display->contrast = 0xCF;
    } 
    
    sendSingleCommand(display, DISPLAY_SETCOMPINS);
    sendSingleCommand(display, comPins);
    sendSingleCommand(display, DISPLAY_SETCONTRAST);
    sendSingleCommand(display, display->contrast);
    
    sendSingleCommand(display, DISPLAY_SETPRECHARGE); // 0xd9
    sendSingleCommand(display, 0xF1);

    uint8_t init5[] = {
        DISPLAY_SETVCOMDETECT, // 0xDB
        0x40,
        DISPLAY_DISPLAYALLON_RESUME, // 0xA4
        DISPLAY_NORMALDISPLAY,       // 0xA6
        DISPLAY_DEACTIVATE_SCROLL,
        DISPLAY_DISPLAYON // Main screen turn on
    }; 

    sendCommandList(display, init5, sizeof(init5) / sizeof(init5[0]));
}

void initI2CBus(I2CBus *bus, i2c_master_bus_config_t *busConfig) {
    assert(bus);
    assert(busConfig);
        
    ESP_ERROR_CHECK(i2c_new_master_bus(busConfig, &bus->handle));
    bus->devicesOnBus = 0;
}

void initI2CDevice(I2CDevice *device, I2CBus *bus, i2c_device_config_t *deviceConfig) {
    assert(device);
    assert(bus);
    assert(deviceConfig);
        
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus->handle, deviceConfig, &device->handle));
    bus->devicesOnBus++;
    device->bus = bus;
}

void destroyDisplay(DisplayDevice *display) {
    if (!display) {
        return;
    }

    destroyDevice(&display->device);

    free(display->buffer);

    display->width = display->height = 0;
}

void destroyDevice(I2CDevice *device) {
    if (!device) {
        return;
    }

    if (device->handle) {
        ESP_ERROR_CHECK(i2c_master_bus_rm_device(device->handle));
        device->handle = NULL;
    }

    if (device->bus) {
        device->bus->devicesOnBus--;
       device->bus = NULL;
    }

}

void destroyI2CBus(I2CBus *bus) {
    if (!bus) {
        return;
    }

    if (bus->handle) {
        ESP_ERROR_CHECK(i2c_del_master_bus(bus->handle));
        bus->handle = NULL;
    }
    bus->devicesOnBus = 0;
}
