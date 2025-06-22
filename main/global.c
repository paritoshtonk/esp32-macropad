#include "global.h"

QueueHandle_t button_queue;
bool isDeviceConnected = false;
bool isHIDTaskStarted = false;

void init_queue()
{
    button_queue = xQueueCreate(10, sizeof(button_event_t));
}