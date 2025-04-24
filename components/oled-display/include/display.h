#ifndef SCREEN_H_
#define SCREEN_H_

#include <driver/i2c_master.h>
#include <stdint.h>

#define DISPLAY_TAG "DISPLAY"

typedef struct {
    i2c_master_bus_handle_t handle;
    size_t devicesOnBus;
} I2CBus;

typedef struct {
    I2CBus *bus;
    i2c_master_dev_handle_t handle;
} I2CDevice;

typedef enum {
    DISPLAY_128_64,
    DISPLAY_128_32,
} DisplayType;

typedef struct {
    I2CDevice device;

    DisplayType type;

    uint8_t width;
    uint8_t height;

    uint8_t contrast;

    uint8_t *dataControlByte;
    uint8_t *buffer;
} DisplayDevice;

typedef enum {
    DISPLAY_COLOR_WHITE,
    DISPLAY_COLOR_BLACK,
    DISPLAY_COLOR_INVERSE,
} DisplayColor;

typedef enum {
    ALIGNMENT_LEFT,
    ALIGNMENT_RIGHT,
} DisplayAlignment;

// Control byte is [co|dc|0|0|0|0|0|0]
// co - continuation bit. If co == 0, the transmission of the following information will contain data bytes only
// dc - data / command selection bit. If dc == 0, the following data byte is a command. If dc == 0 the following data byte as a data which will be stored at the GDDRAM.
//The GDDRAM column address pointer will be increased by one automatically after each data write.
static const uint8_t kCommandControlByte = 0x00;
static const uint8_t kDataControlByte = 0x40;

void initDisplay(DisplayDevice *display, I2CDevice *i2cDevice, DisplayType type);
void initI2CBus(I2CBus *bus, i2c_master_bus_config_t *busConfig);
void initI2CDevice(I2CDevice *device, I2CBus *bus, i2c_device_config_t *deviceConfig);

void destroyDisplay(DisplayDevice *display);
void destroyDevice(I2CDevice *device);
void destroyBus(I2CBus *device);

void sendCommandList(DisplayDevice *display, uint8_t *commands, size_t len);
void sendSingleCommand(DisplayDevice *display, uint8_t command);
void sendData(DisplayDevice *display, uint8_t *data, size_t len);

void displayBuffer(DisplayDevice *display);
void clearBuffer(DisplayDevice *display);

void setPixel(DisplayDevice *display, uint8_t x, uint8_t y, DisplayColor color);
void drawString(DisplayDevice *display, const char *string, uint8_t len, uint8_t row, uint8_t leftBorder, uint8_t rightBorder, DisplayAlignment alignment);
void drawStringFullLine(DisplayDevice *display, const char *string, uint8_t row, DisplayAlignment alignment);
void eraseRowPart(DisplayDevice *display, uint8_t row, uint8_t start, uint8_t end);

// Display commands (taken from https://github.com/adafruit/Adafruit_SSD1306/blob/master/Adafruit_SSD1306.h)
#define DISPLAY_MEMORYMODE 0x20          ///< See datasheet
#define DISPLAY_COLUMNADDR 0x21          ///< See datasheet
#define DISPLAY_PAGEADDR 0x22            ///< See datasheet
#define DISPLAY_SETCONTRAST 0x81         ///< See datasheet
#define DISPLAY_CHARGEPUMP 0x8D          ///< See datasheet
#define DISPLAY_SEGREMAP 0xA0            ///< See datasheet
#define DISPLAY_DISPLAYALLON_RESUME 0xA4 ///< See datasheet
#define DISPLAY_DISPLAYALLON 0xA5        ///< Not currently used
#define DISPLAY_NORMALDISPLAY 0xA6       ///< See datasheet
#define DISPLAY_INVERTDISPLAY 0xA7       ///< See datasheet
#define DISPLAY_SETMULTIPLEX 0xA8        ///< See datasheet
#define DISPLAY_DISPLAYOFF 0xAE          ///< See datasheet
#define DISPLAY_DISPLAYON 0xAF           ///< See datasheet
#define DISPLAY_COMSCANINC 0xC0          ///< Not currently used
#define DISPLAY_COMSCANDEC 0xC8          ///< See datasheet
#define DISPLAY_SETDISPLAYOFFSET 0xD3    ///< See datasheet
#define DISPLAY_SETDISPLAYCLOCKDIV 0xD5  ///< See datasheet
#define DISPLAY_SETPRECHARGE 0xD9        ///< See datasheet
#define DISPLAY_SETCOMPINS 0xDA          ///< See datasheet
#define DISPLAY_SETVCOMDETECT 0xDB       ///< See datasheet

#define DISPLAY_SETLOWCOLUMN 0x00  ///< Not currently used
#define DISPLAY_SETHIGHCOLUMN 0x10 ///< Not currently used
#define DISPLAY_SETSTARTLINE 0x40  ///< See datasheet

#define DISPLAY_EXTERNALVCC 0x01  ///< External display voltage source
#define DISPLAY_SWITCHCAPVCC 0x02 ///< Gen. display voltage from 3.3V

#define DISPLAY_RIGHT_HORIZONTAL_SCROLL 0x26              ///< Init rt scroll
#define DISPLAY_LEFT_HORIZONTAL_SCROLL 0x27               ///< Init left scroll
#define DISPLAY_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL 0x29 ///< Init diag scroll
#define DISPLAY_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL 0x2A  ///< Init diag scroll
#define DISPLAY_DEACTIVATE_SCROLL 0x2E                    ///< Stop scroll
#define DISPLAY_ACTIVATE_SCROLL 0x2F                      ///< Start scroll
#define DISPLAY_SET_VERTICAL_SCROLL_AREA 0xA3             ///< Set scroll range

#endif
