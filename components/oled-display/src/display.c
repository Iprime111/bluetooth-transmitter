#include <assert.h>
#include <driver/i2c_master.h>
#include <esp_err.h>
#include <esp_log.h>
#include <stdint.h>
#include <string.h>

#include "renew_font.h"
#include "display.h"

static uint8_t charToFontIndex(char c);

void drawString(DisplayDevice *display, const char *string, uint8_t row) {
    assert(display);
    assert(string);

    if (row >= display->height / 8) {
        ESP_LOGE(DISPLAY_TAG, "Screen row %u is bigger than available %u", row, display->height / 8);
        return;
    }

    size_t idx = 0;
    size_t bufferCol = 7;

    while (string[idx] != '\0') {
        if (bufferCol + kFontWidth >= display->width) {
            break;
        }

        char symbol = string[idx];

        for (uint8_t fontCol = 0; fontCol < kFontWidth; fontCol++) {
            display->buffer[bufferCol + row * display->width] = font[charToFontIndex(symbol)][fontCol];
            bufferCol++;
        }

        idx++;
    }
}

void setPixel(DisplayDevice *display, uint8_t x, uint8_t y, DisplayColor color) {
    size_t idx = x + (y / 8) * display->width; // Set segment
    uint8_t mask = 1 << (y & 7); // Set bit in segment

    switch (color) {
    case DISPLAY_COLOR_WHITE:
        display->buffer[idx] |= mask; 
        break;
    case DISPLAY_COLOR_BLACK:
        display->buffer[idx] &= ~mask; 
        break;
    case DISPLAY_COLOR_INVERSE:
        display->buffer[idx] |= mask; 
        break;
    }

}

void clearBuffer(DisplayDevice *display) {
    assert(display);
    memset(display->buffer, 0, display->width * display->height / 8);
}

void displayBuffer(DisplayDevice *display) {
    assert(display);

    uint8_t commands[] = {
        DISPLAY_PAGEADDR,
        0,                  // Page start address
        0xFF,               // Page end (not really, but works here)
        DISPLAY_COLUMNADDR, // Column start address
        0,
    };
    sendCommandList(display, commands, sizeof(commands) / sizeof(commands[0]));
    sendSingleCommand(display, display->width - 1);

    sendData(display, display->dataControlByte, display->width * display->height / 8);
}

void sendCommandList(DisplayDevice *display, uint8_t *commands, size_t len) {
    uint8_t controlByte = kCommandControlByte;

    i2c_master_transmit_multi_buffer_info_t commandsBuffer[] = {
        {.write_buffer = &controlByte, .buffer_size = 1},
        {.write_buffer = commands, .buffer_size = len},
    };

    size_t bufferSize = sizeof(commandsBuffer) / sizeof(commandsBuffer[0]);
        
    ESP_ERROR_CHECK(i2c_master_multi_buffer_transmit(display->device.handle, commandsBuffer, bufferSize, -1));
}

void sendSingleCommand(DisplayDevice *display, uint8_t command) {
    assert(display);

    uint8_t commandBytes[2] = {kCommandControlByte, command};
    uint8_t dataLen = sizeof(commandBytes) / sizeof(commandBytes[0]);

    sendData(display, commandBytes, dataLen);
}

void sendData(DisplayDevice *display, uint8_t *data, size_t len) {
    assert(display);
    ESP_ERROR_CHECK(i2c_master_transmit(display->device.handle, data, len, -1));
}

static uint8_t charToFontIndex(char c) {
    c = c & 0x7F;
    if (c < ' ') {
        c = 0;
    } else {
        c -= ' ';
    }

    return c;
}
