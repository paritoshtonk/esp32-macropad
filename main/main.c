
#include "esp_hid_device.c"
#include "global.h"
#include "button.c"
#include "dip.c"

void app_main(void)
{
    init_queue();
    dip_main();
    button_main();
    esp_hid_device_main();
}