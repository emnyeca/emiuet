#include "usb_cc_detect.h"

#include "board_pins.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "usb_power.h"

/* USB_CC_1A5_N is the wired-OR of two TLV7022 comparators: low while either CC
 * is above the Rp 1.5 A threshold. An unpowered comparator or open output reads
 * Default; an output stuck low or a GPIO37 short to GND reads 1.5 A.
 *
 * Type-C requires a Sink to reduce current within tSinkAdj (60 ms max) after an
 * Rp change and allows tRpValueChange (10-20 ms) to qualify it. A drop is taken
 * after two consecutive samples; a raise needs 100 ms so PD BMC traffic that
 * survives the RC filter cannot grant extra current.
 */
#define CC_POLL_MS               10
#define CC_DROP_SAMPLES          2
#define CC_RAISE_SAMPLES         10
/* Use the highest priority so current-advertisement sampling takes precedence
 * over ordinary transport work. Each sample is followed by a blocking wait.
 */
#define CC_TASK_PRIORITY         (configMAX_PRIORITIES - 1)

static const char *TAG = "usb_cc";
static TaskHandle_t s_task;

static void cc_task(void *arg)
{
    (void)arg;
    usb_current_mode_t reported = USB_CURRENT_UNKNOWN;
    usb_current_mode_t candidate = USB_CURRENT_UNKNOWN;
    unsigned stable = 0;

    for (;;) {
        const usb_current_mode_t sample = gpio_get_level(PIN_USB_CC_1A5_N) == 0
                                              ? USB_CURRENT_1P5A_OR_MORE
                                              : USB_CURRENT_DEFAULT;
        if (sample != candidate) {
            stable = 1;
        } else if (stable < CC_RAISE_SAMPLES) {
            ++stable;
        }
        candidate = sample;

        const unsigned needed = sample == USB_CURRENT_1P5A_OR_MORE ? CC_RAISE_SAMPLES
                                                                   : CC_DROP_SAMPLES;
        if (sample != reported && stable >= needed) {
            reported = sample;
            usb_power_set_advertised_current(reported);
            ESP_LOGI(TAG, "advertised=%s rgb_budget=%u mA",
                     reported == USB_CURRENT_1P5A_OR_MORE ? ">=1.5A" : "Default",
                     usb_power_get_status().rgb_budget_ma);
        }
        vTaskDelay(pdMS_TO_TICKS(CC_POLL_MS));
    }
}

bool usb_cc_detect_start(void)
{
    if (s_task) return true;
    return xTaskCreatePinnedToCore(cc_task, "usb_cc", 3072, NULL, CC_TASK_PRIORITY,
                                   &s_task, 0) == pdPASS;
}
