#include "matrix_scan.h"
#include "midi_out.h"
#include "midi_mpe.h"
#include "esp_log.h"
#include "esp_system.h"
#include <stdlib.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Debug: simulate key presses when no physical keys connected.
 * Simulate random notes with random press/release durations to better
 * exercise MIDI and UI code.
 */
/* Forward-declare on_key_event so debug simulator can call it before the
 * real definition appears. */
static void on_key_event(int row, int col, bool pressed);

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
    /* Enable simulator mode and start per-string sim tasks so OLED and MIDI
     * observe simulated presses without being overwritten by hw scan. */
    matrix_scan_set_sim_enabled(true);
    matrix_sim_start();
    ESP_LOGI(TAG, "Started debug matrix simulator (per-string async)");
    ESP_LOGI(TAG, "matrix->MIDI bridge started (discard_cycles=%d)", discard_cycles);
}

void matrix_midi_bridge_stop(void)
{
    matrix_scan_stop();
}
