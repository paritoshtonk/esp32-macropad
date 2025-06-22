#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "global.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"

#define NUM_BUTTONS 5
#define TOTAL_BUTTONS 5
#define LONG_PRESS_THRESHOLD 1000 * 1000 // 1 second in microseconds
#define DEBOUNCE_TIME_MS 20

static const char *BUTTON_TAG = "BUTTON";
typedef struct
{
    gpio_num_t gpio;
    char id_char;
    int64_t press_time_us;
    TaskHandle_t task_handle;
} button_t;

// === CONFIGURATION ===
const gpio_num_t gpio_pins[TOTAL_BUTTONS] = {GPIO_NUM_1, GPIO_NUM_2, GPIO_NUM_3, GPIO_NUM_4, GPIO_NUM_5};
const char button_chars[TOTAL_BUTTONS] = {'u', 'r', 'd', 'l', 'c'};

button_t buttons[NUM_BUTTONS];

// === ISR: Notify on both edges ===
static void IRAM_ATTR button_isr_handler(void *arg)
{
    gpio_num_t gpio_num = (gpio_num_t)(int)arg;
    for (int i = 0; i < NUM_BUTTONS; i++)
    {
        if (buttons[i].gpio == gpio_num)
        {
            int level = gpio_get_level(gpio_num);
            int64_t now = esp_timer_get_time();

            if (level == 0)
            {
                buttons[i].press_time_us = now;
            }
            else
            {
                BaseType_t hpTaskWoken = pdFALSE;
                vTaskNotifyGiveFromISR(buttons[i].task_handle, &hpTaskWoken);
                if (hpTaskWoken)
                    portYIELD_FROM_ISR();
            }
            break;
        }
    }
}

// === Button press handling task ===
void button_task(void *arg)
{
    button_t *btn = (button_t *)arg;

    while (1)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // Debounce: wait and confirm release
        vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_TIME_MS));
        if (gpio_get_level(btn->gpio) != 1)
            continue;

        int64_t release_time = esp_timer_get_time();
        int64_t duration = release_time - btn->press_time_us;

        button_event_t evt = {
            .id_char = btn->id_char,
            .long_press = (duration >= LONG_PRESS_THRESHOLD)};

        xQueueSend(button_queue, &evt, 0);
    }
}

void button_main(void)
{
    gpio_install_isr_service(0);

    // Init all buttons
    for (int i = 0; i < NUM_BUTTONS; i++)
    {
        buttons[i].gpio = gpio_pins[i];
        buttons[i].id_char = button_chars[i];
        buttons[i].press_time_us = 0;
        // Configure button GPIO
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << buttons[i].gpio),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_ANYEDGE // Detect press + release
        };
        gpio_config(&io_conf);

        xTaskCreate(button_task, "button_task", 2048, &buttons[i], 10, &buttons[i].task_handle);

        // Register ISR
        gpio_isr_handler_add(buttons[i].gpio, button_isr_handler, (void *)(int)buttons[i].gpio);
    }

    ESP_LOGI(BUTTON_TAG, " Buttons with queue initialized");
}
