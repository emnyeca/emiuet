#include "matrix_scan.h"
#include "midi_out.h"
#include "midi_mpe.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Debug: simulate key presses when no physical keys connected.
 * Simulate row=1 (2nd string), col=1 (1st fret): 1s pressed, 1s released.
 */
static TaskHandle_t s_sim_task = NULL;

/* Forward-declare on_key_event so debug simulator can call it before the
 * real definition appears. */
static void on_key_event(int row, int col, bool pressed);

static void sim_key_task(void *arg)
{
    (void)arg;
    const TickType_t press_ticks = pdMS_TO_TICKS(1000);
    const TickType_t release_ticks = pdMS_TO_TICKS(1000);
    while (1) {
        on_key_event(1, 1, true);
        vTaskDelay(press_ticks);
        on_key_event(1, 1, false);
        vTaskDelay(release_ticks);
    }
}

static const char *TAG = "matrix_midi";

/* Default base notes for strings Str1..Str6 (row 0..5)
 * Assumes Str1 is high E (E4=64) and Str6 is low E (E2=40)
 */
static const uint8_t string_base_note[6] = {64, 59, 55, 50, 45, 40};

/* Base MIDI channel for non-MPE mode - use midi_mpe_default_channel() */

static void on_key_event(int row, int col, bool pressed)
{
    if (row < 0 || row >= 6 || col < 0 || col >= 13) return;

    uint8_t note = string_base_note[row] + col;
    if (pressed) {
        /* remember activity for MPE pitch-bend routing */
        midi_mpe_note_activity(row);

        uint8_t ch = midi_mpe_is_enabled() ? midi_mpe_channel_for_row(row) : midi_mpe_default_channel();
        midi_send_note_on(ch, note, 100);
    } else {
        uint8_t ch = midi_mpe_is_enabled() ? midi_mpe_channel_for_row(row) : midi_mpe_default_channel();
        midi_send_note_off(ch, note, 0);
    }
}

void matrix_midi_bridge_start(int discard_cycles)
{
    midi_out_init();
    midi_mpe_init();
    matrix_scan_start(on_key_event, discard_cycles);
    /* Start debug key simulator so we can test MIDI behavior without hardware. */
    if (!s_sim_task) {
        /* Increase stack to avoid vfprintf/printf-related stack overflow in
         * low-stack contexts. 4096 bytes is safer for tasks that call
         * logging and MIDI send helpers. */
        xTaskCreate(sim_key_task, "sim_key", 4096, NULL, 5, &s_sim_task);
        ESP_LOGI(TAG, "Started debug key simulator (row=1,col=1 1s on/1s off)");
    }
    ESP_LOGI(TAG, "matrix->MIDI bridge started (discard_cycles=%d)", discard_cycles);
}

void matrix_midi_bridge_stop(void)
{
    matrix_scan_stop();
}
