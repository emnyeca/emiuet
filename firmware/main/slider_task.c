#include "slider.h"
#include "midi_mpe.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "slider_task";

static TaskHandle_t s_task = NULL;

static void slider_task(void *arg)
{
    (void)arg;
    const TickType_t delay = pdMS_TO_TICKS(10);

    while (1) {
        uint16_t raw = slider_read_pitchbend(); /* 0..1023 */

        /* Map 0..1023 to 14-bit MIDI value with center at 8192.
         * Upward-only bend: 0 -> 8192, 1023 -> 16383
         */
        const int MIDI_CENTER = 8192;
        const int MIDI_MAX = 16383;
        uint32_t mapped = MIDI_CENTER + ((uint32_t)raw * (MIDI_MAX - MIDI_CENTER) / 1023U);
        if (mapped > MIDI_MAX) mapped = MIDI_MAX;

        midi_mpe_apply_pitchbend((uint16_t)mapped);

        vTaskDelay(delay);
    }
}

void slider_task_start(void)
{
    if (s_task) return;
    slider_init();
    xTaskCreate(slider_task, "slider_task", 4096, NULL, 6, &s_task);
    ESP_LOGI(TAG, "slider task started");
}
