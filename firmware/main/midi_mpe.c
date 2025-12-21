
#include "midi_mpe.h"
#include "midi_out.h"
#include <stdbool.h>
#include "esp_log.h"

static const char *TAG = "midi_mpe";

static bool g_mpe_enabled = false;
static int g_last_active_row = 0;

void midi_mpe_init(void)
{
    g_mpe_enabled = false;
    g_last_active_row = 0;
}

void midi_mpe_set_enabled(bool en)
{
    g_mpe_enabled = en;
}

bool midi_mpe_is_enabled(void)
{
    return g_mpe_enabled;
}

void midi_mpe_note_activity(int row)
{
    if (row >= 0 && row < 6) g_last_active_row = row;
}

/* bend_value: 0..16383 (14-bit). Interpretation: 8192 == center.
 * This device uses upward-only bends: callers should map controller input
 * such that 0 -> center (8192) and max -> 16383.
 */
void midi_mpe_apply_pitchbend(uint16_t bend_value)
{
    uint8_t channel;
    if (g_mpe_enabled) {
        /* Map last active row to MPE channel base (MIDI_MPE_BASE_CHANNEL defined in bridge). */
        channel = (uint8_t)(1 + g_last_active_row);
    } else {
        channel = 0; /* default channel 0 */
    }

    midi_send_pitchbend(channel, bend_value);
    ESP_LOGD(TAG, "apply_pitchbend ch=%d value=%d", channel, bend_value);
}
