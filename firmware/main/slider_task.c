#include "slider.h"
#include "midi_mpe.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "slider_task";
static TaskHandle_t s_task = NULL;

#define SLIDER_PB_POLL_MS      10
#define SLIDER_PB_DEADBAND     12   /* 8〜16くらいで好み調整 */
/* Minimum interval between pitch-bend sends (ms) to avoid flooding */
#define SLIDER_PB_MIN_SEND_MS  20
/* When raw <= this value treat as bottom (center) snap. Tune by feel. */
#define SLIDER_PB_BOTTOM_RAW   16

static void slider_task(void *arg)
{
    (void)arg;
    const TickType_t delay = pdMS_TO_TICKS(SLIDER_PB_POLL_MS);

    uint16_t last_sent = 0xFFFF; /* impossible init */
    TickType_t last_send_tick = 0; /* rate-limit last send time */
    const TickType_t min_send_ticks = pdMS_TO_TICKS(SLIDER_PB_MIN_SEND_MS);

    while (1) {
        uint16_t raw = slider_read_pitchbend(); /* 0..1023 */

        const int MIDI_CENTER = 8192;
        const int MIDI_MAX = 16383;
        uint32_t mapped = MIDI_CENTER + ((uint32_t)raw * (MIDI_MAX - MIDI_CENTER) / 1023U);
        if (mapped > MIDI_MAX) mapped = MIDI_MAX;

        uint16_t cur = (uint16_t)mapped;
        /* Bottom snap: if slider is at (near) bottom, force center */
        const bool is_bottom = (raw <= SLIDER_PB_BOTTOM_RAW);
        if (is_bottom) {
            cur = MIDI_CENTER;
        }

        /* MPE PB lock state local to this task — we inform midi_mpe when
         * we need to lock/unlock. */
        static bool pb_target_locked = false;

        /* send only if changed enough */
        if (last_sent == 0xFFFF) {
            last_sent = cur; /* initialize without sending burst */
        } else {
            int diff = (int)cur - (int)last_sent;
            if (diff < 0) diff = -diff;

            if (diff >= SLIDER_PB_DEADBAND) {
                TickType_t now = xTaskGetTickCount();

                /* Always allow sending when we reached center (bottom snap) so
                 * the PB target can be unlocked/reset immediately. Otherwise
                 * obey rate limit. */
                bool allow_send = is_bottom || ((now - last_send_tick) >= min_send_ticks);

                if (allow_send) {
                    /* If MPE mode: when we send a non-center value, lock the
                     * PB target so note-activity doesn't change it mid-bend. */
                    if (midi_mpe_is_enabled()) {
                        if (cur != MIDI_CENTER && !pb_target_locked) {
                            midi_mpe_lock_pitchbend_target(true);
                            pb_target_locked = true;
                        }
                    }

                    midi_mpe_apply_pitchbend(cur);
                    last_send_tick = now;
                    last_sent = cur;

                    /* If we sent center (either by snap or by movement), then
                     * unlock and reset PB target so next bend picks current
                     * last-active string. */
                    if (cur == MIDI_CENTER && pb_target_locked && midi_mpe_is_enabled()) {
                        midi_mpe_lock_pitchbend_target(false);
                        midi_mpe_reset_pitchbend_target();
                        pb_target_locked = false;
                    }
                } else {
                    /* Skip sending due to rate limit; keep last_sent so the
                     * next allowed send will reflect the newest value. */
                }
            }
        }

        vTaskDelay(delay);
    }
}

void slider_task_start(void)
{
    if (s_task) return;

    slider_init();
    if (!slider_is_enabled()) {
        ESP_LOGW(TAG, "slider disabled; slider task not started");
        return;
    }

    xTaskCreate(slider_task, "slider_task", 4096, NULL, 6, &s_task);
    ESP_LOGD(TAG, "slider task started");
}
