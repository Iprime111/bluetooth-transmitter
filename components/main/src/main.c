#include <math.h>
#include <esp_adc/adc_continuous.h>
#include <soc/soc_caps.h>
#include <esp_log.h>
#include <hal/adc_types.h>
#include <driver/i2c_master.h>
#include <driver/gpio.h>
#include <soc/gpio_num.h>
#include <stdint.h>
#include <string.h>

#include "audio_stream.h"
#include "bt_lib.h"
#include "display.h"
#include "encoder.h"
#include "portmacro.h"
#include "menu.h"
#include "stdbool.h"

#define kMaxFramesRequested (256)
#define kChannelsCount (2)
#define kChannelFrameSize (sizeof(uint32_t))

#define kDisplayAddress (0x3c) // 0x3c for 32-pixels tall displays, 0x3d for others

#define kI2CMasterScl (GPIO_NUM_5)
#define kI2CMasterSda (GPIO_NUM_17)

#define kEncoderAPort (GPIO_NUM_35)
#define kEncoderBPort (GPIO_NUM_34)
#define kEncoderCPort (GPIO_NUM_32)

#define kAudioFrequency (44100)

static uint8_t *audioDataBuffer = NULL;
static InputAudioStream stream = {};

static int32_t audioDataCallback(AudioFrame *data, int32_t len);

void app_main() {
    // Init display
    i2c_master_bus_config_t masterBusConfig = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = -1, // Auto selection
        .scl_io_num = kI2CMasterScl,
        .sda_io_num = kI2CMasterSda,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    I2CBus screenBus;
    initI2CBus(&screenBus, &masterBusConfig);

    i2c_device_config_t deviceConfig = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = kDisplayAddress,
        .scl_speed_hz = 100000,
    };

    I2CDevice displayI2C;
    initI2CDevice(&displayI2C, &screenBus, &deviceConfig);

    DisplayDevice display;
    initDisplay(&display, &displayI2C, DISPLAY_128_32);
    setMenuDisplay(&display);

    // Init encoder
    Encoder encoder;
    initEncoder(&encoder, kEncoderAPort, kEncoderBPort, kEncoderCPort);
    setEncoderCallback(&encoder, encoderCallback, NULL);

    // Init I2S
    InputAudioStreamConfig audioStreamConfig = {
        .samplingFrequency = kAudioFrequency,
        .dinPort = GPIO_NUM_15,
        .bclkPort = GPIO_NUM_2,
        .mclkPort = GPIO_NUM_0,
        .wsPort = GPIO_NUM_4,
        .readTimeout = 1000,
    };
    
    audioDataBuffer = calloc(kMaxFramesRequested * kChannelFrameSize * kChannelsCount, sizeof(uint8_t));
    initInputAudioStream(&stream, &audioStreamConfig);

    // Init bluetooth
    BluetoothDeviceCallbacks btCallbacks = {
        .audioDataCallback = audioDataCallback,
        .deviceStateChangedCallback = handleDeviceStateChangedEvent,
        .audioStateChangedCallback = NULL,
        .deviceDiscoveredCallback = handleDeviceDiscoveredEvent,
        .volumeChangedCallback = volumeChangedCallback,
    };

    initBtDevice(&btCallbacks);

    while (true) {
        vTaskDelay(portMAX_DELAY);
    }
    
    free(audioDataBuffer);
    destroyEncoder(&encoder);
    destroyDisplay(&display);
    destroyI2CBus(&screenBus);
}

// 44.1kHz, dual channel (16 bits every frame channel => 32 bits every frame)
static int32_t audioDataCallback(AudioFrame *data, int32_t len) {

    // Every data slot is 32-bytes (for each channel). But we need only 16 of them.
    // Let's take higher 16 bits from every slot
    uint16_t *readData = (uint16_t *)audioDataBuffer;

    readAudioData(&stream, audioDataBuffer, len * 2 * sizeof(uint32_t), NULL);

    for (size_t frameIdx = 0; frameIdx < len; ++frameIdx) {
        data[frameIdx].channel1 = readData[frameIdx * 4 + 1];
        data[frameIdx].channel2 = readData[(frameIdx * 4) + 3];
    }

    return len;
}
