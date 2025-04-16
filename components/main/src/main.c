#include <math.h>

#include "bt_lib.h"

#define c3_frequency  130.81

// 44.1kHz, dual channel (16 bits every channel record => 32 bits every chunk)
static int32_t audioDataCallback(uint8_t *data, int32_t len) {
    if (data == NULL || len < 0) {
        return 0;
    }

    int16_t *p_buf = (int16_t *)data;
    for (int i = 0; i < (len >> 1); i++) {
        p_buf[i] = rand() % (1 << 16);
    }

    return len;
}

void app_main() {
    initBtDevice(audioDataCallback);
}
