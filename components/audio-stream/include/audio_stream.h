#ifndef SOUND_STREAM_H_
#define SOUND_STREAM_H_

#include <hal/gpio_types.h>
#include <driver/i2s_types.h>
#include <soc/soc_caps.h>
#include <stdint.h>
#include <stddef.h>

typedef struct {
    gpio_port_t mclkPort;
    gpio_port_t bclkPort;
    gpio_port_t wsPort;
    gpio_port_t dinPort;

    uint32_t samplingFrequency;

    int readTimeout;
} InputAudioStreamConfig;

typedef struct {
    i2s_chan_handle_t rxHandle;

    InputAudioStreamConfig config;
} InputAudioStream;

void initInputAudioStream(InputAudioStream *stream, InputAudioStreamConfig *config);
// TODO dtor
void readAudioData(InputAudioStream *stream, uint8_t *buffer, size_t bufferSize, size_t *readBytes);

#endif
