#include <assert.h>
#include <esp_log.h>
#include <freertos/projdefs.h>
#include <freertos/idf_additions.h>
#include <portmacro.h>
#include <string.h>

#include "dispatcher.h"

#define DISPATCHER_TAG "DISPATCHER"

#define kDefaultStackDepth (4096)
#define kDispatcherPriority (10)
#define kMaxQueueWaitTimeMs (10)
#define kQueueLength (10)

typedef struct {
    uint32_t event;
    void *param;
    DispatcherTask callback;
} DispatcherMessage;

static bool sendDispatcherMessage(Dispatcher *dispatcher, DispatcherMessage *message);
static void taskHandler(void *dispatcherPtr);


static void taskHandler(void *dispatcherPtr) {
    assert(dispatcherPtr);

    Dispatcher *dispatcher = (Dispatcher *)dispatcherPtr;
    DispatcherMessage message;

    while (true) {
        if (xQueueReceive(dispatcher->taskQueue, &message, (TickType_t)portMAX_DELAY) != pdTRUE) {
            continue;
        }

        if (message.callback) {
            message.callback(message.event, message.param);
        }

        if (message.param) {
            free(message.param);
        }
    }
}

static bool sendDispatcherMessage(Dispatcher *dispatcher, DispatcherMessage *message) {
    assert(dispatcher);
    
    if (!message) {
        return false;
    }

    if (xQueueSend(dispatcher->taskQueue, message, kMaxQueueWaitTimeMs / portTICK_PERIOD_MS) != pdTRUE) {
        ESP_LOGE(DISPATCHER_TAG, "Task dispatch failed failed");
        return false;
    }

    return true;
}

bool dispatchTask(Dispatcher *dispatcher, DispatcherTask callback, uint16_t event, void *param, size_t paramLen) {
    assert(dispatcher);

    DispatcherMessage message = {
        .event = event,
        .param = NULL,
        .callback = callback,
    };

    if (paramLen == 0) {
        return sendDispatcherMessage(dispatcher, &message);
    } else if (param && paramLen > 0) {
        message.param = calloc(1, paramLen);

        if (!message.param) {
            return false;
        }

        memcpy(message.param, param, paramLen);
        return sendDispatcherMessage(dispatcher, &message);
    }

    return false;
}

void initDispatcher(Dispatcher *dispatcher) {
    dispatcher->taskQueue = xQueueCreate(kQueueLength, sizeof(DispatcherMessage));
    xTaskCreate(taskHandler, "Dispatcher", kDefaultStackDepth, dispatcher, kDispatcherPriority, &dispatcher->taskHandle);
}

void destroyDispatcher(Dispatcher *dispatcher) {
    if (dispatcher->taskHandle) {
        vTaskDelete(dispatcher->taskHandle);
        dispatcher->taskHandle = NULL;
    }

    if (dispatcher->taskQueue) {
        vQueueDelete(dispatcher->taskQueue);
        dispatcher->taskQueue = NULL;
    }
}

