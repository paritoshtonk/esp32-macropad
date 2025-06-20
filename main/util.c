#include "util.h"

void printDebugLog(char *TAG, char *message)
{
    ESP_LOGD(TAG, "%s", message);
}