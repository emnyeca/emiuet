#include "slider.h"
#include "midi_mpe.h"
#include "board_pins.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "slider_task";
static TaskHandle_t s_task = NULL;

#define SLIDER_PB_POLL_MS      10
#define SLIDER_PB_DEADBAND     12   /* 8〜16くらいで好み調整 */
/* Minimum interval between pitch-bend sends (ms) to avoid flooding */
/* Minimum interval between pitch-bend sends (ms) to avoid flooding when
 * rapid changes occur. Sending still only happens on meaningful diffs or
 * events (bottom snap). No unconditional periodic sends. */
#define SLIDER_PB_MIN_SEND_MS  20
/* When raw <= this value treat as bottom (center) snap. Tune by feel. */
#define SLIDER_PB_BOTTOM_RAW   16
/* ADC/sample/filter tuning */
#ifndef SLIDER_ADC_SAMPLES
#define SLIDER_ADC_SAMPLES 8
#endif

#define SLIDER_SEND_DIFF_THRESHOLD 12 /* difference (mapped units) required to send */
/* State-machine thresholds (tuneable) */
#define SLIDER_START_THRESHOLD  16   /* raw units to consider movement start */
#define SLIDER_START_COUNT      2    /* consecutive polls above start threshold */
#define SLIDER_STOP_THRESHOLD   8    /* raw units for settling/stop */
#define SLIDER_STOP_COUNT       6    /* consecutive polls within stop threshold */

static void slider_task(void *arg)
{
    (void)arg;
    const TickType_t delay = pdMS_TO_TICKS(SLIDER_PB_POLL_MS);

    uint16_t last_sent = 0xFFFF; /* impossible init */
    TickType_t last_send_tick = 0; /* rate-limit last send time */
    const TickType_t min_send_ticks = pdMS_TO_TICKS(SLIDER_PB_MIN_SEND_MS);

    enum { SLIDER_STATE_IDLE = 0, SLIDER_STATE_ACTIVE, SLIDER_STATE_SETTLE } state = SLIDER_STATE_IDLE;
    uint16_t baseline = 0;
    int start_count = 0;
    int stop_count = 0;
    uint16_t pending_value = 0;
    static bool pb_target_locked = false;
    bool just_activated = false;

    while (1) {
        uint16_t raw = slider_read_pitchbend(); /* 0..1023 */
        /* Poll SW_CENTER (PIN_SW_CENTER) for MPE toggle/debug. Detect edges. */
        static int last_sw_center = 1;
        int sw_now = gpio_get_level(PIN_SW_CENTER);
        if (sw_now != last_sw_center) {
            /* simple debounce: require stable for 30ms (polled at 10ms) */
            static int stable_count = 0;
            if (sw_now == last_sw_center) {
                stable_count = 0;
            } else {
                stable_count++;
            }
            if (stable_count >= 3) {
                /* falling edge = pressed (active low) */
                if (sw_now == 0) {
                    bool new_en = !midi_mpe_is_enabled();
                    midi_mpe_set_enabled(new_en);
                    ESP_LOGI(TAG, "SW_CENTER pressed: MPE %s", new_en ? "ENABLED" : "DISABLED");
                }
                last_sw_center = sw_now;
                stable_count = 0;
            }
        }

        const int MIDI_CENTER = 8192;
        const int MIDI_MAX = 16383;
        uint32_t mapped = MIDI_CENTER + ((uint32_t)raw * (MIDI_MAX - MIDI_CENTER) / 1023U);
        if (mapped > MIDI_MAX) mapped = MIDI_MAX;

        uint16_t cur = (uint16_t)mapped;
        /* Bottom snap: if slider is at (near) bottom, force center */
        const bool is_bottom = (raw <= SLIDER_PB_BOTTOM_RAW);
        if (is_bottom) cur = MIDI_CENTER;

        /* State machine for sending */
        switch (state) {
        case SLIDER_STATE_IDLE: {
            /* Initialize baseline on first pass */
            if (baseline == 0) baseline = cur;

            int delta = (int)cur - (int)baseline;
            if (delta < 0) delta = -delta;
            if (delta >= SLIDER_START_THRESHOLD) {
                start_count++;
            } else {
                start_count = 0;
            }

            if (start_count >= SLIDER_START_COUNT) {
                state = SLIDER_STATE_ACTIVE;
                pending_value = cur;
                start_count = 0;
                stop_count = 0;
                just_activated = true;
                /* reset rate timer so immediate send on activation is allowed */
                last_send_tick = 0;
                ESP_LOGI(TAG, "slider: ACTIVE (baseline=%u)", (unsigned)baseline);
            }
            break;
        }

        case SLIDER_STATE_ACTIVE: {
            /* Movement ongoing; update pending. Send at rate limit. */
            pending_value = cur;
            TickType_t now = xTaskGetTickCount();

            /* Bottom-handling: always send exactly one center and return to IDLE */
            if (is_bottom) {
                if (last_sent != MIDI_CENTER) {
                    /* send center immediately */
                    if (midi_mpe_is_enabled() && !pb_target_locked) {
                        /* ensure lock/unlock semantics: lock briefly so reset behaves */
                        midi_mpe_lock_pitchbend_target(true);
                        pb_target_locked = true;
                    }
                    midi_mpe_apply_pitchbend(MIDI_CENTER);
                    ESP_LOGD(TAG, "PB bottom snap -> center sent");
                    last_sent = MIDI_CENTER;
                    /* unlock/reset and go idle */
                    if (pb_target_locked && midi_mpe_is_enabled()) {
                        midi_mpe_lock_pitchbend_target(false);
                        midi_mpe_reset_pitchbend_target();
                        pb_target_locked = false;
                    }
                }
                state = SLIDER_STATE_IDLE;
                baseline = MIDI_CENTER;
                break;
            }

            /* Decide whether to send: only if difference is meaningful */
            int send_diff = (int)pending_value - (int)last_sent; if (send_diff < 0) send_diff = -send_diff;
            bool should_send = false;
            if (just_activated) {
                /* when movement first detected, send immediately */
                should_send = true;
            } else if (last_sent == 0xFFFF) {
                /* no prior sends yet */
                should_send = true;
            } else if (send_diff >= SLIDER_SEND_DIFF_THRESHOLD) {
                should_send = true;
            }

            if (should_send) {
                if ((now - last_send_tick) >= min_send_ticks || just_activated) {
                    /* On first non-center send, lock MPE target */
                    if (midi_mpe_is_enabled()) {
                        if (pending_value != MIDI_CENTER && !pb_target_locked) {
                            midi_mpe_lock_pitchbend_target(true);
                            pb_target_locked = true;
                        }
                    }

                    midi_mpe_apply_pitchbend(pending_value);
                    int last_ch = midi_mpe_get_last_active_channel();
                    ESP_LOGD(TAG, "PB send raw=%u cur=%u bottom=%d locked=%d last_ch=%d",
                             (unsigned)raw, (unsigned)pending_value, (int)is_bottom,
                             (int)pb_target_locked, last_ch);
                    last_send_tick = now;
                    last_sent = pending_value;

                    /* If center sent (shouldn't happen here), unlock/reset PB target immediately */
                    if (pending_value == MIDI_CENTER && pb_target_locked && midi_mpe_is_enabled()) {
                        midi_mpe_lock_pitchbend_target(false);
                        midi_mpe_reset_pitchbend_target();
                        pb_target_locked = false;
                    }
                }
            }

            just_activated = false;

            /* detect settle: last_sent close to current reading */
            int diff = (int)cur - (int)last_sent; if (diff < 0) diff = -diff;
            if (diff <= SLIDER_STOP_THRESHOLD) {
                stop_count++;
            } else {
                stop_count = 0;
            }

            if (stop_count >= SLIDER_STOP_COUNT) {
                state = SLIDER_STATE_SETTLE;
                ESP_LOGI(TAG, "slider: SETTLE (last_sent=%u)", (unsigned)last_sent);
                stop_count = 0;
            }
            break;
        }

        case SLIDER_STATE_SETTLE: {
            /* Wait briefly to confirm stop, then go idle and adopt baseline */
            int diff = (int)cur - (int)last_sent; if (diff < 0) diff = -diff;
            if (diff <= SLIDER_STOP_THRESHOLD) {
                stop_count++;
            } else {
                /* movement resumed */
                state = SLIDER_STATE_ACTIVE;
                stop_count = 0;
                break;
            }

            if (stop_count >= SLIDER_STOP_COUNT) {
                state = SLIDER_STATE_IDLE;
                baseline = cur;
                last_sent = cur; /* avoid immediate resend */
                stop_count = 0;
                ESP_LOGI(TAG, "slider: IDLE (new baseline=%u)", (unsigned)baseline);
            }
            break;
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

    /* Configure center switch (PIN_SW_CENTER) for debug MPE toggle/logging */
    gpio_config_t io = {0};
    io.mode = GPIO_MODE_INPUT;
    io.pin_bit_mask = (1ULL << PIN_SW_CENTER);
    io.pull_up_en = 1;
    io.pull_down_en = 0;
    gpio_config(&io);

    xTaskCreate(slider_task, "slider_task", 4096, NULL, 6, &s_task);
    ESP_LOGD(TAG, "slider task started");
}
