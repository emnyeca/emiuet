#include "usb_hid_keyboard.h"

#include <string.h>

#include "sdkconfig.h"

#if defined(CONFIG_TINYUSB_HID_COUNT) && (CONFIG_TINYUSB_HID_COUNT > 0)
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "tusb.h"
#include "class/hid/hid_device.h"
#endif

static volatile uint8_t s_keyboard_leds = 0;

#if defined(CONFIG_TINYUSB_HID_COUNT) && (CONFIG_TINYUSB_HID_COUNT > 0)
#define USB_HID_REPORT_QUEUE_LEN 64

typedef struct {
    uint8_t modifiers;
    uint8_t keycodes[USB_HID_KEYBOARD_MAX_KEYS];
} keyboard_report_t;

static QueueHandle_t s_report_queue = NULL;
static TaskHandle_t s_sender_task = NULL;

static void hid_sender_task(void *arg)
{
    (void)arg;
    while (1) {
        keyboard_report_t report = {0};
        if (!tud_mounted() || !tud_hid_ready() || !s_report_queue ||
            xQueuePeek(s_report_queue, &report, pdMS_TO_TICKS(1)) != pdTRUE) {
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        if (tud_hid_keyboard_report(0, report.modifiers, report.keycodes)) {
            (void)xQueueReceive(s_report_queue, &report, 0);
        } else {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}
#endif

bool usb_hid_keyboard_init(void)
{
#if defined(CONFIG_TINYUSB_HID_COUNT) && (CONFIG_TINYUSB_HID_COUNT > 0)
    if (!s_report_queue) {
        s_report_queue = xQueueCreate(USB_HID_REPORT_QUEUE_LEN, sizeof(keyboard_report_t));
        if (!s_report_queue) return false;
    }
    if (!s_sender_task) {
        if (xTaskCreatePinnedToCore(hid_sender_task, "hid_keyboard_tx", 3072, NULL, 6,
                                    &s_sender_task, 0) != pdPASS) {
            s_sender_task = NULL;
            return false;
        }
    }
    return true;
#else
    return false;
#endif
}

bool usb_hid_keyboard_send_report(uint8_t modifiers,
                                  const uint8_t keycodes[USB_HID_KEYBOARD_MAX_KEYS])
{
#if defined(CONFIG_TINYUSB_HID_COUNT) && (CONFIG_TINYUSB_HID_COUNT > 0)
    if (!s_report_queue || !tud_mounted()) return false;

    keyboard_report_t report = {.modifiers = modifiers};
    for (int i = 0; i < USB_HID_KEYBOARD_MAX_KEYS; ++i) report.keycodes[i] = keycodes[i];

    if (xQueueSendToBack(s_report_queue, &report, 0) == pdTRUE) return true;

    /* Preserve a safe final state under pathological input bursts. */
    (void)xQueueReset(s_report_queue);
    return xQueueSendToBack(s_report_queue, &report, 0) == pdTRUE;
#else
    (void)modifiers;
    (void)keycodes;
    return false;
#endif
}

void usb_hid_keyboard_release_all(void)
{
    const uint8_t empty[USB_HID_KEYBOARD_MAX_KEYS] = {0};
#if defined(CONFIG_TINYUSB_HID_COUNT) && (CONFIG_TINYUSB_HID_COUNT > 0)
    if (s_report_queue) (void)xQueueReset(s_report_queue);
#endif
    (void)usb_hid_keyboard_send_report(0, empty);
}

void usb_hid_keyboard_disconnected(void)
{
#if defined(CONFIG_TINYUSB_HID_COUNT) && (CONFIG_TINYUSB_HID_COUNT > 0)
    if (s_report_queue) (void)xQueueReset(s_report_queue);
#endif
    s_keyboard_leds = 0;
}

void usb_hid_keyboard_set_led_report(uint8_t leds)
{
    s_keyboard_leds = leds;
}

bool usb_hid_keyboard_caps_lock_on(void)
{
    /* HID keyboard LED output bit 1 is Caps Lock. */
    return (s_keyboard_leds & (1u << 1)) != 0;
}
