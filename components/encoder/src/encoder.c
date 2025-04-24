#include <driver/gpio.h>
#include <esp_attr.h>
#include <stdint.h>

#include "encoder.h"
#include "esp_err.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "hal/gpio_types.h"
#include "portmacro.h"

static const size_t kEncoderQueueSize = 40;
static const size_t kTaskStackDepth = 2048;
static const size_t kTaskPriority = 10;
static const TickType_t kDebounceTimerPeriod = 50 / portTICK_PERIOD_MS;

static void IRAM_ATTR isrABPortHandler(void *param);
static void IRAM_ATTR isrSwitchPortHandler(void *param);

static void isrABPortHandler(void *param) {
    Encoder *encoder = param;

    ISRParam queueValue = {
        .port = ENCODER_AB,
        .aState = gpio_get_level(encoder->aPort),
        .bState = gpio_get_level(encoder->bPort),
    };

    xQueueSendFromISR(encoder->queue, &queueValue, 0);
}

static void isrSwitchPortHandler(void *param) {
    Encoder *encoder = param;

    gpio_isr_handler_remove(encoder->switchPort);
    xTimerStart(encoder->switchDebounceTimer, 0);
}

static void switchPortDebounceCheck(TimerHandle_t timer) {
    Encoder *encoder = pvTimerGetTimerID(timer);

    if (gpio_get_level(encoder->switchPort) == 1) {
        ESP_ERROR_CHECK(gpio_isr_handler_add(encoder->switchPort, isrSwitchPortHandler, encoder));
        return;
    }

    ISRParam queueValue = {
        .port = ENCODER_SWITCH,
        .aState = 0,
        .bState = 0,
    };

    ESP_ERROR_CHECK(gpio_isr_handler_add(encoder->switchPort, isrSwitchPortHandler, encoder));
    xQueueSend(encoder->queue, &queueValue, 0);
}

static void monitoringTask(void* param) {
    Encoder *encoder = param;
    ISRParam isrParam;

    while (true) {
        if (xQueueReceive(encoder->queue, &isrParam, portMAX_DELAY) == pdFALSE) {
            continue;
        }

        if (isrParam.port == ENCODER_SWITCH) {
            if (encoder->callback) {
                encoder->callback(ENCODER_SWITCH_PRESSED, encoder->callbackParam);
            }
            continue;
        }

        uint8_t currentState = (isrParam.bState << 1) | isrParam.aState;
        uint8_t lastState = encoder->lastStates & 0x3; // lastStates & 0b00000011

        if (currentState == lastState) {
            continue;
        }

        encoder->lastStates = (encoder->lastStates << 2) | currentState; // Store current state
                                        
        if (!encoder->callback) {
            continue;
        }

        // Check lastStates field documentation
        if (encoder->lastStates == 0x87) { // 0b10000111
            encoder->callback(ENCODER_STEP_CCW, encoder->callbackParam);
        } else if (encoder->lastStates == 0x4b) { // 0b01001011
            encoder->callback(ENCODER_STEP_CW, encoder->callbackParam);
        }
    }
}

void setEncoderCallback(Encoder *encoder, EncoderCallback callback, void *callbackParam) {
    assert(encoder);

    encoder->callback = callback;
    encoder->callbackParam = callbackParam;
}

// ISR service should be started
void initEncoder(Encoder *encoder, gpio_num_t aPort, gpio_num_t bPort, gpio_num_t switchPort) {
    assert(encoder);

    uint64_t aMask = 1ull << aPort;
    uint64_t bMask = 1ull << bPort;
    uint64_t cMask = 1ull << switchPort;

    encoder->aPort = aPort;
    encoder->bPort = bPort;
    encoder->switchPort = switchPort;

    encoder->callback = NULL;
    encoder->callbackParam = NULL;
    encoder->lastStates = 0;

    encoder->switchDebounceTimer = xTimerCreate("Debounce timer", kDebounceTimerPeriod, pdFALSE, encoder, switchPortDebounceCheck);

    gpio_config_t gpioConfig = {
        .intr_type = GPIO_INTR_ANYEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = aMask | bMask | cMask,
        .pull_down_en = 0,
        .pull_up_en = 0,
    };

    ESP_ERROR_CHECK(gpio_config(&gpioConfig));
    
    gpio_set_intr_type(switchPort, GPIO_INTR_NEGEDGE);

    encoder->queue = xQueueCreate(kEncoderQueueSize, sizeof(ISRParam));
    
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(aPort, isrABPortHandler, encoder));
    ESP_ERROR_CHECK(gpio_isr_handler_add(bPort, isrABPortHandler, encoder));
    ESP_ERROR_CHECK(gpio_isr_handler_add(switchPort, isrSwitchPortHandler, encoder));
    
    xTaskCreate(monitoringTask, "MonitoringTask", kTaskStackDepth, encoder, kTaskPriority, NULL);
}

void destroyEncoder(Encoder *encoder) {
    if (!encoder) {
        return;
    }

    gpio_isr_handler_remove(encoder->aPort);
    gpio_isr_handler_remove(encoder->bPort);
    gpio_isr_handler_remove(encoder->switchPort);

    encoder->aPort = encoder->bPort = encoder->switchPort = -1;

    if (encoder->monitoringTask) {
        vTaskDelete(encoder->monitoringTask);
        encoder->monitoringTask = NULL;
    }

    if (encoder->queue) {
        vQueueDelete(encoder->queue);
        encoder->queue = NULL;
    }

    encoder->callback = NULL;
    encoder->callbackParam = NULL;
    encoder->lastStates = -1;
}
