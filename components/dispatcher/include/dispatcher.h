#ifndef DISPATCHER_H_
#define DISPATCHER_H_

#include <freertos/idf_additions.h>

typedef struct {
    QueueHandle_t taskQueue;
    TaskHandle_t taskHandle;
} Dispatcher;

typedef void (*DispatcherTask)(uint16_t event, void *param);

bool dispatchTask(Dispatcher *dispatcher, DispatcherTask callback, uint16_t event, void *param, size_t paramLen);
void initDispatcher(Dispatcher *dispatcher);
void destroyDispatcher(Dispatcher *dispatcher);

#endif
