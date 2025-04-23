#ifndef ENCODER_H_
#define ENCODER_H_

#include <soc/gpio_num.h>
#include <freertos/idf_additions.h>

typedef enum {
    ENCODER_STEP_CW,
    ENCODER_STEP_CCW,
    ENCODER_SWITCH_PRESSED,
} EncoderEvent;

typedef void (* EncoderCallback)(EncoderEvent, void *);

typedef struct {
    QueueHandle_t queue;

    uint8_t aPort;
    uint8_t bPort;
    uint8_t switchPort;
    
    // Here we're storing 4 last states of A and B ports like [B|A|B|A|B|A|B|A]
    // We need 4 last states due to encoder output signal form https://wiki.dfrobot.com/EC11_Rotary_Encoder_Module_SKU__SEN0235
    uint8_t lastStates;

    EncoderCallback callback;
    void *callbackParam;

    TaskHandle_t monitoringTask;

    TimerHandle_t switchDebounceTimer;
} Encoder;

typedef enum {
    ENCODER_AB,
    ENCODER_SWITCH,
} EncoderPort;

typedef struct {
    EncoderPort port;
    bool aState;
    bool bState;
} ISRParam;

void initEncoder(Encoder *encoder, gpio_num_t aPort, gpio_num_t bPort, gpio_num_t cPort);
void destroyEncoder(Encoder *encoder);

void setEncoderCallback(Encoder *encoder, EncoderCallback callback, void *param);

#endif
