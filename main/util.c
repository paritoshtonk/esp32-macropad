#include "util.h"

void printDebugLog(char *TAG, char *message)
{
    const char *default_tag = "UTIL";

    if (TAG == NULL || strlen(TAG) == 0)
    {
        TAG = (char *)default_tag;
    }

    ESP_LOGD(TAG, "%s", message);
}