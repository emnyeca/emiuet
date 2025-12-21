#include "slider.h"
#include "midi_mpe.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "slider_task";
static TaskHandle_t s_task = NULL;

#define SLIDER_PB_POLL_MS      10
#define SLIDER_PB_DEADBAND     12   /* 8〜16くらいで好み調整 */

static void slider_task(void *arg)
{
    (void)arg;
    const TickType_t delay = pdMS_TO_TICKS(SLIDER_PB_POLL_MS);

    uint16_t last_sent = 0xFFFF; /* impossible init */

    while (1) {
        uint16_t raw = slider_read_pitchbend(); /* 0..1023 */

        const int MIDI_CENTER = 8192;
        const int MIDI_MAX = 16383;
        uint32_t mapped = MIDI_CENTER + ((uint32_t)raw * (MIDI_MAX - MIDI_CENTER) / 1023U);
        if (mapped > MIDI_MAX) mapped = MIDI_MAX;

        uint16_t cur = (uint16_t)mapped;

        /* send only if changed enough */
        if (last_sent == 0xFFFF) {
            last_sent = cur; /* initialize without sending burst */
        } else {
            int diff = (int)cur - (int)last_sent;
            if (diff < 0) diff = -diff;

            if (diff >= SLIDER_PB_DEADBAND) {
                midi_mpe_apply_pitchbend(cur);
                last_sent = cur;
            }
        }

        vTaskDelay(delay);
    }
}

void slider_task_start(void)
{
    if (s_task) return;

    slider_init();

    /* ここが重要：sliderが無効なら起動しない（次の節で実装） */
    xTaskCreate(slider_task, "slider_task", 4096, NULL, 6, &s_task);
    ESP_LOGI(TAG, "slider task started");
}
