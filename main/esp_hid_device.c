/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"

#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

#include "esp_hidd.h"
#include "esp_hid_gap.h"

static const char *TAG = "HID_DEV_DEMO";

typedef struct
{
    TaskHandle_t task_hdl;
    esp_hidd_dev_t *hid_dev;
    uint8_t protocol_mode;
    uint8_t *buffer;
} local_param_t;

int counter = 0;

static local_param_t s_ble_hid_param = {0};

// send the buttons, change in x, and change in y
void send_mouse(uint8_t buttons, char dx, char dy, char wheel)
{
    static uint8_t buffer[4] = {0};
    buffer[0] = buttons;
    buffer[1] = dx;
    buffer[2] = dy;
    buffer[3] = wheel;
    esp_hidd_dev_input_set(s_ble_hid_param.hid_dev, 0, 0, buffer, 4);
}

void ble_hid_demo_task_mouse(void *pvParameters)
{
    static const char *help_string = "########################################################################\n"
                                     "BT hid mouse demo usage:\n"
                                     "You can input these value to simulate mouse: 'q', 'w', 'e', 'a', 's', 'd', 'h'\n"
                                     "q -- click the left key\n"
                                     "w -- move up\n"
                                     "e -- click the right key\n"
                                     "a -- move left\n"
                                     "s -- move down\n"
                                     "d -- move right\n"
                                     "h -- show the help\n"
                                     "########################################################################\n";
    printf("%s\n", help_string);
    char c;
    while (1)
    {
        c = fgetc(stdin);
        switch (c)
        {
        case 'q':
            send_mouse(1, 0, 0, 0);
            break;
        case 'w':
            send_mouse(0, 0, -10, 0);
            break;
        case 'e':
            send_mouse(2, 0, 0, 0);
            break;
        case 'a':
            send_mouse(0, -10, 0, 0);
            break;
        case 's':
            send_mouse(0, 0, 10, 0);
            break;
        case 'd':
            send_mouse(0, 10, 0, 0);
            break;
        case 'h':
            printf("%s\n", help_string);
            break;
        default:
            break;
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

#define CASE(a, b, c)  \
    case a:            \
        buffer[0] = b; \
        buffer[2] = c; \
        break;

// USB keyboard codes
#define USB_HID_MODIFIER_LEFT_CTRL 0x01
#define USB_HID_MODIFIER_LEFT_SHIFT 0x02
#define USB_HID_MODIFIER_LEFT_ALT 0x04
#define USB_HID_MODIFIER_RIGHT_CTRL 0x10
#define USB_HID_MODIFIER_RIGHT_SHIFT 0x20
#define USB_HID_MODIFIER_RIGHT_ALT 0x40

#define USB_HID_SPACE 0x2C
#define USB_HID_DOT 0x37
#define USB_HID_NEWLINE 0x28
#define USB_HID_FSLASH 0x38
#define USB_HID_BSLASH 0x31
#define USB_HID_COMMA 0x36
#define USB_HID_DOT 0x37

const unsigned char keyboardReportMap[] = {
    // 7 bytes input (modifiers, resrvd, keys*5), 1 byte output
    0x05, 0x01, // Usage Page (Generic Desktop Ctrls)
    0x09, 0x06, // Usage (Keyboard)
    0xA1, 0x01, // Collection (Application)
    0x85, 0x01, //   Report ID (1)
    0x05, 0x07, //   Usage Page (Kbrd/Keypad)
    0x19, 0xE0, //   Usage Minimum (0xE0)
    0x29, 0xE7, //   Usage Maximum (0xE7)
    0x15, 0x00, //   Logical Minimum (0)
    0x25, 0x01, //   Logical Maximum (1)
    0x75, 0x01, //   Report Size (1)
    0x95, 0x08, //   Report Count (8)
    0x81, 0x02, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x01, //   Report Count (1)
    0x75, 0x08, //   Report Size (8)
    0x81, 0x03, //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x05, //   Report Count (5)
    0x75, 0x01, //   Report Size (1)
    0x05, 0x08, //   Usage Page (LEDs)
    0x19, 0x01, //   Usage Minimum (Num Lock)
    0x29, 0x05, //   Usage Maximum (Kana)
    0x91, 0x02, //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x95, 0x01, //   Report Count (1)
    0x75, 0x03, //   Report Size (3)
    0x91, 0x03, //   Output (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x95, 0x05, //   Report Count (5)
    0x75, 0x08, //   Report Size (8)
    0x15, 0x00, //   Logical Minimum (0)
    0x25, 0x65, //   Logical Maximum (101)
    0x05, 0x07, //   Usage Page (Kbrd/Keypad)
    0x19, 0x00, //   Usage Minimum (0x00)
    0x29, 0x65, //   Usage Maximum (0x65)
    0x81, 0x00, //   Input (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,       // End Collection

    // 65 bytes
};

static void char_to_code(uint8_t *buffer, char ch)
{
    // Check if lower or upper case
    if (ch >= 'a' && ch <= 'z')
    {
        buffer[0] = 0;
        // convert ch to HID letter, starting at a = 4
        buffer[2] = (uint8_t)(4 + (ch - 'a'));
    }
    else if (ch >= 'A' && ch <= 'Z')
    {
        // Add left shift
        buffer[0] = USB_HID_MODIFIER_LEFT_SHIFT;
        // convert ch to lower case
        ch = ch - ('A' - 'a');
        // convert ch to HID letter, starting at a = 4
        buffer[2] = (uint8_t)(4 + (ch - 'a'));
    }
    else if (ch >= '0' && ch <= '9') // Check if number
    {
        buffer[0] = 0;
        // convert ch to HID number, starting at 1 = 30, 0 = 39
        if (ch == '0')
        {
            buffer[2] = 39;
        }
        else
        {
            buffer[2] = (uint8_t)(30 + (ch - '1'));
        }
    }
    else // not a letter nor a number
    {
        switch (ch)
        {
            CASE(' ', 0, USB_HID_SPACE);
            CASE('.', 0, USB_HID_DOT);
            CASE('\n', 0, USB_HID_NEWLINE);
            CASE('?', USB_HID_MODIFIER_LEFT_SHIFT, USB_HID_FSLASH);
            CASE('/', 0, USB_HID_FSLASH);
            CASE('\\', 0, USB_HID_BSLASH);
            CASE('|', USB_HID_MODIFIER_LEFT_SHIFT, USB_HID_BSLASH);
            CASE(',', 0, USB_HID_COMMA);
            CASE('<', USB_HID_MODIFIER_LEFT_SHIFT, USB_HID_COMMA);
            CASE('>', USB_HID_MODIFIER_LEFT_SHIFT, USB_HID_COMMA);
            CASE('@', USB_HID_MODIFIER_LEFT_SHIFT, 31);
            CASE('!', USB_HID_MODIFIER_LEFT_SHIFT, 30);
            CASE('#', USB_HID_MODIFIER_LEFT_SHIFT, 32);
            CASE('$', USB_HID_MODIFIER_LEFT_SHIFT, 33);
            CASE('%', USB_HID_MODIFIER_LEFT_SHIFT, 34);
            CASE('^', USB_HID_MODIFIER_LEFT_SHIFT, 35);
            CASE('&', USB_HID_MODIFIER_LEFT_SHIFT, 36);
            CASE('*', USB_HID_MODIFIER_LEFT_SHIFT, 37);
            CASE('(', USB_HID_MODIFIER_LEFT_SHIFT, 38);
            CASE(')', USB_HID_MODIFIER_LEFT_SHIFT, 39);
            CASE('-', 0, 0x2D);
            CASE('_', USB_HID_MODIFIER_LEFT_SHIFT, 0x2D);
            CASE('=', 0, 0x2E);
            CASE('+', USB_HID_MODIFIER_LEFT_SHIFT, 39);
            CASE(8, 0, 0x2A); // backspace
            CASE('\t', 0, 0x2B);
        default:
            buffer[0] = 0;
            buffer[2] = 0;
        }
    }
}

void send_keyboard(char c)
{
    static uint8_t buffer[8] = {0};
    char_to_code(buffer, c);
    esp_hidd_dev_input_set(s_ble_hid_param.hid_dev, 0, 1, buffer, 8);
    /* send the keyrelease event with sufficient delay */
    vTaskDelay(50 / portTICK_PERIOD_MS);
    memset(buffer, 0, sizeof(uint8_t) * 8);
    esp_hidd_dev_input_set(s_ble_hid_param.hid_dev, 0, 1, buffer, 8);
}

void ble_hid_demo_task_kbd(void *pvParameters)
{
    static const char *help_string = "########################################################################\n"
                                     "BT hid keyboard demo usage:\n"
                                     "########################################################################\n";
    /* TODO : Add support for function keys and ctrl, alt, esc, etc. */
    printf("%s\n", help_string);
    // char c;
    char *buffer = "abcdef";
    int i = 0;
    while (i < 6)
    {
        send_keyboard(buffer[i]);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        i++;
    }

    vTaskDelay(5000 / portTICK_PERIOD_MS);
    counter++;
    if (counter <= 10)
        ble_hid_demo_task_kbd(NULL);
}

static esp_hid_raw_report_map_t ble_report_maps[] = {
    {.data = keyboardReportMap,
     .len = sizeof(keyboardReportMap)},
};

static esp_hid_device_config_t ble_hid_config = {
    .vendor_id = 0x16C0,
    .product_id = 0x05DF,
    .version = 0x0100,
    .device_name = "ESP Keyboard",
    .manufacturer_name = "Espressif",
    .serial_number = "1234567890",
    .report_maps = ble_report_maps,
    .report_maps_len = 1};

#define HID_CC_RPT_MUTE 1
#define HID_CC_RPT_POWER 2
#define HID_CC_RPT_LAST 3
#define HID_CC_RPT_ASSIGN_SEL 4
#define HID_CC_RPT_PLAY 5
#define HID_CC_RPT_PAUSE 6
#define HID_CC_RPT_RECORD 7
#define HID_CC_RPT_FAST_FWD 8
#define HID_CC_RPT_REWIND 9
#define HID_CC_RPT_SCAN_NEXT_TRK 10
#define HID_CC_RPT_SCAN_PREV_TRK 11
#define HID_CC_RPT_STOP 12

#define HID_CC_RPT_CHANNEL_UP 0x10
#define HID_CC_RPT_CHANNEL_DOWN 0x30
#define HID_CC_RPT_VOLUME_UP 0x40
#define HID_CC_RPT_VOLUME_DOWN 0x80

// HID Consumer Control report bitmasks
#define HID_CC_RPT_NUMERIC_BITS 0xF0
#define HID_CC_RPT_CHANNEL_BITS 0xCF
#define HID_CC_RPT_VOLUME_BITS 0x3F
#define HID_CC_RPT_BUTTON_BITS 0xF0
#define HID_CC_RPT_SELECTION_BITS 0xCF

// Macros for the HID Consumer Control 2-byte report
#define HID_CC_RPT_SET_NUMERIC(s, x)   \
    (s)[0] &= HID_CC_RPT_NUMERIC_BITS; \
    (s)[0] = (x)
#define HID_CC_RPT_SET_CHANNEL(s, x)   \
    (s)[0] &= HID_CC_RPT_CHANNEL_BITS; \
    (s)[0] |= ((x) & 0x03) << 4
#define HID_CC_RPT_SET_VOLUME_UP(s)   \
    (s)[0] &= HID_CC_RPT_VOLUME_BITS; \
    (s)[0] |= 0x40
#define HID_CC_RPT_SET_VOLUME_DOWN(s) \
    (s)[0] &= HID_CC_RPT_VOLUME_BITS; \
    (s)[0] |= 0x80
#define HID_CC_RPT_SET_BUTTON(s, x)   \
    (s)[1] &= HID_CC_RPT_BUTTON_BITS; \
    (s)[1] |= (x)
#define HID_CC_RPT_SET_SELECTION(s, x)   \
    (s)[1] &= HID_CC_RPT_SELECTION_BITS; \
    (s)[1] |= ((x) & 0x03) << 4

// HID Consumer Usage IDs (subset of the codes available in the USB HID Usage Tables spec)
#define HID_CONSUMER_POWER 48 // Power
#define HID_CONSUMER_RESET 49 // Reset
#define HID_CONSUMER_SLEEP 50 // Sleep

#define HID_CONSUMER_MENU 64          // Menu
#define HID_CONSUMER_SELECTION 128    // Selection
#define HID_CONSUMER_ASSIGN_SEL 129   // Assign Selection
#define HID_CONSUMER_MODE_STEP 130    // Mode Step
#define HID_CONSUMER_RECALL_LAST 131  // Recall Last
#define HID_CONSUMER_QUIT 148         // Quit
#define HID_CONSUMER_HELP 149         // Help
#define HID_CONSUMER_CHANNEL_UP 156   // Channel Increment
#define HID_CONSUMER_CHANNEL_DOWN 157 // Channel Decrement

#define HID_CONSUMER_PLAY 176          // Play
#define HID_CONSUMER_PAUSE 177         // Pause
#define HID_CONSUMER_RECORD 178        // Record
#define HID_CONSUMER_FAST_FORWARD 179  // Fast Forward
#define HID_CONSUMER_REWIND 180        // Rewind
#define HID_CONSUMER_SCAN_NEXT_TRK 181 // Scan Next Track
#define HID_CONSUMER_SCAN_PREV_TRK 182 // Scan Previous Track
#define HID_CONSUMER_STOP 183          // Stop
#define HID_CONSUMER_EJECT 184         // Eject
#define HID_CONSUMER_RANDOM_PLAY 185   // Random Play
#define HID_CONSUMER_SELECT_DISC 186   // Select Disk
#define HID_CONSUMER_ENTER_DISC 187    // Enter Disc
#define HID_CONSUMER_REPEAT 188        // Repeat
#define HID_CONSUMER_STOP_EJECT 204    // Stop/Eject
#define HID_CONSUMER_PLAY_PAUSE 205    // Play/Pause
#define HID_CONSUMER_PLAY_SKIP 206     // Play/Skip

#define HID_CONSUMER_VOLUME 224      // Volume
#define HID_CONSUMER_BALANCE 225     // Balance
#define HID_CONSUMER_MUTE 226        // Mute
#define HID_CONSUMER_BASS 227        // Bass
#define HID_CONSUMER_VOLUME_UP 233   // Volume Increment
#define HID_CONSUMER_VOLUME_DOWN 234 // Volume Decrement

#define HID_RPT_ID_CC_IN 3  // Consumer Control input report ID
#define HID_CC_IN_RPT_LEN 2 // Consumer Control input report Len
void esp_hidd_send_consumer_value(uint8_t key_cmd, bool key_pressed)
{
    uint8_t buffer[HID_CC_IN_RPT_LEN] = {0, 0};
    if (key_pressed)
    {
        switch (key_cmd)
        {
        case HID_CONSUMER_CHANNEL_UP:
            HID_CC_RPT_SET_CHANNEL(buffer, HID_CC_RPT_CHANNEL_UP);
            break;

        case HID_CONSUMER_CHANNEL_DOWN:
            HID_CC_RPT_SET_CHANNEL(buffer, HID_CC_RPT_CHANNEL_DOWN);
            break;

        case HID_CONSUMER_VOLUME_UP:
            HID_CC_RPT_SET_VOLUME_UP(buffer);
            break;

        case HID_CONSUMER_VOLUME_DOWN:
            HID_CC_RPT_SET_VOLUME_DOWN(buffer);
            break;

        case HID_CONSUMER_MUTE:
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_MUTE);
            break;

        case HID_CONSUMER_POWER:
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_POWER);
            break;

        case HID_CONSUMER_RECALL_LAST:
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_LAST);
            break;

        case HID_CONSUMER_ASSIGN_SEL:
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_ASSIGN_SEL);
            break;

        case HID_CONSUMER_PLAY:
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_PLAY);
            break;

        case HID_CONSUMER_PAUSE:
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_PAUSE);
            break;

        case HID_CONSUMER_RECORD:
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_RECORD);
            break;

        case HID_CONSUMER_FAST_FORWARD:
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_FAST_FWD);
            break;

        case HID_CONSUMER_REWIND:
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_REWIND);
            break;

        case HID_CONSUMER_SCAN_NEXT_TRK:
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_SCAN_NEXT_TRK);
            break;

        case HID_CONSUMER_SCAN_PREV_TRK:
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_SCAN_PREV_TRK);
            break;

        case HID_CONSUMER_STOP:
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_STOP);
            break;

        default:
            break;
        }
    }
    esp_hidd_dev_input_set(s_ble_hid_param.hid_dev, 0, HID_RPT_ID_CC_IN, buffer, HID_CC_IN_RPT_LEN);
    return;
}

void ble_hid_task_start_up(void)
{
    if (s_ble_hid_param.task_hdl)
    {
        // Task already exists
        return;
    }
    /* Nimble Specific */
    xTaskCreate(ble_hid_demo_task_kbd, "ble_hid_demo_task_kbd", 3 * 1024, NULL, configMAX_PRIORITIES - 3,
                &s_ble_hid_param.task_hdl);
}

void ble_hid_task_shut_down(void)
{
    if (s_ble_hid_param.task_hdl)
    {
        vTaskDelete(s_ble_hid_param.task_hdl);
        s_ble_hid_param.task_hdl = NULL;
    }
}

static void ble_hidd_event_callback(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    esp_hidd_event_t event = (esp_hidd_event_t)id;
    esp_hidd_event_data_t *param = (esp_hidd_event_data_t *)event_data;
    static const char *TAG = "HID_DEV_BLE";

    switch (event)
    {
    case ESP_HIDD_START_EVENT:
    {
        ESP_LOGI(TAG, "START");
        esp_hid_ble_gap_adv_start();
        break;
    }
    case ESP_HIDD_CONNECT_EVENT:
    {
        ESP_LOGI(TAG, "CONNECT");
        break;
    }
    case ESP_HIDD_PROTOCOL_MODE_EVENT:
    {
        ESP_LOGI(TAG, "PROTOCOL MODE[%u]: %s", param->protocol_mode.map_index, param->protocol_mode.protocol_mode ? "REPORT" : "BOOT");
        break;
    }
    case ESP_HIDD_CONTROL_EVENT:
    {
        ESP_LOGI(TAG, "CONTROL[%u]: %sSUSPEND", param->control.map_index, param->control.control ? "EXIT_" : "");
        if (param->control.control)
        {
            // exit suspend
            ble_hid_task_start_up();
        }
        else
        {
            // suspend
            ble_hid_task_shut_down();
        }
        break;
    }
    case ESP_HIDD_OUTPUT_EVENT:
    {
        ESP_LOGI(TAG, "OUTPUT[%u]: %8s ID: %2u, Len: %d, Data:", param->output.map_index, esp_hid_usage_str(param->output.usage), param->output.report_id, param->output.length);
        ESP_LOG_BUFFER_HEX(TAG, param->output.data, param->output.length);
        break;
    }
    case ESP_HIDD_FEATURE_EVENT:
    {
        ESP_LOGI(TAG, "FEATURE[%u]: %8s ID: %2u, Len: %d, Data:", param->feature.map_index, esp_hid_usage_str(param->feature.usage), param->feature.report_id, param->feature.length);
        ESP_LOG_BUFFER_HEX(TAG, param->feature.data, param->feature.length);
        break;
    }
    case ESP_HIDD_DISCONNECT_EVENT:
    {
        ESP_LOGI(TAG, "DISCONNECT: %s", esp_hid_disconnect_reason_str(esp_hidd_dev_transport_get(param->disconnect.dev), param->disconnect.reason));
        ble_hid_task_shut_down();
        esp_hid_ble_gap_adv_start();
        break;
    }
    case ESP_HIDD_STOP_EVENT:
    {
        ESP_LOGI(TAG, "STOP");
        break;
    }
    default:
        break;
    }
    return;
}

void ble_hid_device_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE Host Task Started");
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();

    nimble_port_freertos_deinit();
}

void ble_store_config_init(void);

void esp_hid_device_main(void)
{
    esp_err_t ret;
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "setting hid gap, mode:%d", HIDD_BLE_MODE);
    ret = esp_hid_gap_init(HIDD_BLE_MODE);
    ESP_ERROR_CHECK(ret);

    ret = esp_hid_ble_gap_adv_init(ESP_HID_APPEARANCE_KEYBOARD, ble_hid_config.device_name);
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "setting ble device");
    ESP_ERROR_CHECK(
        esp_hidd_dev_init(&ble_hid_config, ESP_HID_TRANSPORT_BLE, ble_hidd_event_callback, &s_ble_hid_param.hid_dev));
    /* XXX Need to have template for store */
    ble_store_config_init();

    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    /* Starting nimble task after gatts is initialized*/
    ret = esp_nimble_enable(ble_hid_device_host_task);
    if (ret)
    {
        ESP_LOGE(TAG, "esp_nimble_enable failed: %d", ret);
    }
}
