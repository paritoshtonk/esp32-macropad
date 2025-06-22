#ifndef GLOBAL_H
#define GLOBAL_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

typedef struct
{
    char id_char;
    bool long_press;
} button_event_t;

extern QueueHandle_t button_queue;

extern bool isDeviceConnected;
extern bool isHIDTaskStarted;

void init_queue();

#endif