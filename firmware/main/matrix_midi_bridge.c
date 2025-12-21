#include "matrix_scan.h"
#include "midi_out.h"
#include "midi_mpe.h"
#include "esp_log.h"

static const char *TAG = "matrix_midi";

/* Default base notes for strings Str1..Str6 (row 0..5)
 * Assumes Str1 is high E (E4=64) and Str6 is low E (E2=40)
 */
static const uint8_t string_base_note[6] = {64, 59, 55, 50, 45, 40};

/* Base MIDI channel for non-MPE mode */
#define MIDI_DEFAULT_CHANNEL 0

/* MPE base channel (0-based) when enabled; maps row->channel = MPE_BASE + row */
#define MIDI_MPE_BASE_CHANNEL 1

static void on_key_event(int row, int col, bool pressed)
{
    if (row < 0 || row >= 6 || col < 0 || col >= 13) return;

    uint8_t note = string_base_note[row] + col;
    if (pressed) {
        /* remember activity for MPE pitch-bend routing */
        midi_mpe_note_activity(row);

        uint8_t ch = midi_mpe_is_enabled() ? (MIDI_MPE_BASE_CHANNEL + row) : MIDI_DEFAULT_CHANNEL;
        midi_send_note_on(ch, note, 100);
    } else {
        uint8_t ch = midi_mpe_is_enabled() ? (MIDI_MPE_BASE_CHANNEL + row) : MIDI_DEFAULT_CHANNEL;
        midi_send_note_off(ch, note, 0);
    }
}

void matrix_midi_bridge_start(void)
{
    midi_out_init();
    midi_mpe_init();
    matrix_scan_start(on_key_event);
    ESP_LOGI(TAG, "matrix->MIDI bridge started");
}

void matrix_midi_bridge_stop(void)
{
    matrix_scan_stop();
}
