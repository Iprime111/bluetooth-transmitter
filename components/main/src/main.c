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

#include "bt_lib.h"
#include "display.h"
#include "encoder.h"
#include "portmacro.h"
#include "menu.h"

#define ADC_TAG "ADC_CONFIG"

static const size_t kAdcReadLen = 256;

static const int kScreenAddress = 0x3c; // 0x3c for 32-pixels tall, 0x3d for others

static const int kI2CMasterScl = 4;
static const int kI2CMasterSda = 5;

static const gpio_num_t kEncoderAPort = GPIO_NUM_32;
static const gpio_num_t kEncoderBPort = GPIO_NUM_35;
static const gpio_num_t kEncoderCPort = GPIO_NUM_33;

static int32_t audioDataCallback(AudioFrame *data, int32_t len);
static adc_continuous_handle_t initADC(adc_channel_t *channels, size_t channelsCount);

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

    // TODO separate function
    i2c_device_config_t deviceConfig = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = kScreenAddress,
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

    // Init bluetooth
    BluetoothDeviceCallbacks btCallbacks = {
        .audioDataCallback = audioDataCallback,
        .deviceStateChangedCallback = handleDeviceStateChangedEvent,
        .audioStateChangedCallback = NULL,
        .deviceDiscoveredCallback = handleDeviceDiscoveredEvent,
    };

    initBtDevice(&btCallbacks);
    startDiscovery();

    while (true) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    
    destroyEncoder(&encoder);
    destroyDisplay(&display);
    destroyBus(&screenBus);

    //adc_channel_t adcChannels[] = {ADC_CHANNEL_0};
    //initADC(adcChannels, sizeof(adcChannels) / sizeof(adcChannels[0]));
}

// 44.1kHz, dual channel (16 bits every frame channel => 32 bits every frame)
__attribute__((unused)) static int32_t audioDataCallback(AudioFrame *data, int32_t len) {
    if (data == NULL || len < 0) {
        return 0;
    }

    for (int i = 0; i < len; i++) {
        data[i].channel1 = rand() % (1 << 16);
        data[i].channel2 = data[i].channel1;
    }

    return len;
}

__attribute__((unused)) static adc_continuous_handle_t initADC(adc_channel_t *channels, size_t channelsCount) {

    adc_continuous_handle_t handle = NULL;

    adc_continuous_handle_cfg_t adcConfig = {
        .max_store_buf_size = 1024,
        .conv_frame_size = kAdcReadLen,
    };

    ESP_ERROR_CHECK(adc_continuous_new_handle(&adcConfig, &handle));

    adc_continuous_config_t digitalConfig = {
        .sample_freq_hz = SOC_ADC_SAMPLE_FREQ_THRES_LOW, // from SOC_ADC_SAMPLE_FREQ_THRES_LOW to SOC_ADC_SAMPLE_FREQ_THRES_HIGH
        .conv_mode = ADC_CONV_SINGLE_UNIT_1, // Direct memory access mode
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1, // type1 data format in adc_digi_output_data_t struct
    };

    adc_digi_pattern_config_t adcPattern[SOC_ADC_PATT_LEN_MAX] = {0};

    digitalConfig.pattern_num = channelsCount;

    for (int channelIdx = 0; channelIdx < channelsCount; channelIdx++) {
        adcPattern[channelIdx].atten = ADC_ATTEN_DB_0; // No input attenuation
        adcPattern[channelIdx].channel = channels[channelIdx] & 0x7;
        adcPattern[channelIdx].unit = ADC_UNIT_1;
        adcPattern[channelIdx].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;

        ESP_LOGI(ADC_TAG, "adc_pattern[%d].atten is :%"PRIx8, channelIdx, adcPattern[channelIdx].atten);
        ESP_LOGI(ADC_TAG, "adc_pattern[%d].channel is :%"PRIx8, channelIdx, adcPattern[channelIdx].channel);
        ESP_LOGI(ADC_TAG, "adc_pattern[%d].unit is :%"PRIx8, channelIdx, adcPattern[channelIdx].unit);
    }

    digitalConfig.adc_pattern = adcPattern;
    ESP_ERROR_CHECK(adc_continuous_config(handle, &digitalConfig));

    return handle;
}

