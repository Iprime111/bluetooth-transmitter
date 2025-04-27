#include <esp_err.h>
#include <esp_log.h>
#include <soc/soc_caps.h>
#include <driver/i2s_common.h>
#include <driver/i2s_std.h>
#include <stdint.h>

#include "audio_stream.h"
#include "hal/i2s_types.h"

#define INPUT_AUDIO_STREAM_TAG "INPUT_AUDIO_STREAM"

#define kI2SUnit (0)


void initInputAudioStream(InputAudioStream *stream, InputAudioStreamConfig *config) {
    assert(stream);
    assert(config);

    if (stream->rxHandle) {
        ESP_LOGE(INPUT_AUDIO_STREAM_TAG, "Stream has already been initialized");
    }

    // Noooo, you can't just skip all safety checks before initializing I2S
    // Haha, initialization goes brrr
    
    i2s_chan_config_t rxChannelConfig = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&rxChannelConfig, NULL, &stream->rxHandle));

    i2s_std_config_t rxStdConfig = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(config->samplingFrequency),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = config->mclkPort,
            .bclk = config->bclkPort,
            .ws   = config->wsPort,
            .dout = I2S_GPIO_UNUSED,
            .din  = config->dinPort,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    rxStdConfig.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(stream->rxHandle, &rxStdConfig));
    
    ESP_ERROR_CHECK(i2s_channel_enable(stream->rxHandle));

    stream->config = *config;
}

void readAudioData(InputAudioStream *stream, uint8_t *buffer, size_t bufferSize, size_t *readBytes) {
    assert(stream);
    assert(buffer);
    
    ESP_ERROR_CHECK(i2s_channel_read(stream->rxHandle, buffer, bufferSize, readBytes, stream->config.readTimeout));
}
