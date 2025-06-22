#include <stdio.h>
#include "driver/gpio.h"
#include "esp_log.h"

#define DIP_TAG "DIP_STARTUP"

#define NUM_SWITCHES 4
const gpio_num_t dip_gpio[NUM_SWITCHES] = {GPIO_NUM_9, GPIO_NUM_8, GPIO_NUM_7, GPIO_NUM_44};

void dip_main(void)
{
    // 1. Configure DIP GPIOs as input with pull-up
    for (int i = 0; i < NUM_SWITCHES; i++)
    {
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << dip_gpio[i]),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE};
        gpio_config(&io_conf);
    }

    // 2. Read DIP switch state once
    uint8_t dip_state = 0;
    for (int i = 0; i < NUM_SWITCHES; i++)
    {
        int level = gpio_get_level(dip_gpio[i]);
        ESP_LOGI(DIP_TAG, "DIP state at startup: %d", level);
        dip_state |= (!level) << i; // Invert: ON=0 becomes 1
    }

    ESP_LOGI(DIP_TAG, "DIP state at startup: 0x%X (binary: %04b)", dip_state, dip_state);

    // 3. Use the state for conditional behavior
    if (dip_state == 0b0001)
    {
        ESP_LOGI(DIP_TAG, "Mode: TEST MODE");
    }
    else if (dip_state == 0b0010)
    {
        ESP_LOGI(DIP_TAG, "Mode: DEBUG MODE");
    }
    else
    {
        ESP_LOGI(DIP_TAG, "Mode: NORMAL MODE");
    }

    // Continue with app logic...
}
