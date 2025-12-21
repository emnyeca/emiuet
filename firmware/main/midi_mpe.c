
#include "midi_mpe.h"
#include "midi_out.h"
#include <stdbool.h>
#include "esp_log.h"

static const char *TAG = "midi_mpe";

static bool g_mpe_enabled = false;
static int g_last_active_row = -1; /* -1 == no last-active row (reset state) */
static uint8_t g_mpe_base_channel = 1; /* default MPE base channel */

/* When true, note activity will NOT update g_last_active_row. This is used
 * to lock the pitch-bend target while a bend is in progress. */
static bool g_pb_locked = false;

/* default non-MPE channel (0-based) */
static const uint8_t g_default_channel = 0;

void midi_mpe_init(void)
{
    g_mpe_enabled = false;
    g_last_active_row = -1;
    g_pb_locked = false;
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
    /* Update last-active row only when not locked. If locked, keep the
     * existing target until unlocked by slider logic. */
    if (!g_pb_locked && row >= 0 && row < 6) g_last_active_row = row;
}

/* bend_value: 0..16383 (14-bit). Interpretation: 8192 == center.
 * This device uses upward-only bends: callers should map controller input
 * such that 0 -> center (8192) and max -> 16383.
 */
void midi_mpe_apply_pitchbend(uint16_t bend_value)
{
    uint8_t channel = g_default_channel;
    if (g_mpe_enabled) {
        if (g_last_active_row >= 0) {
            channel = midi_mpe_channel_for_row(g_last_active_row);
        } else {
            /* No last-active string selected: fall back to default channel */
            channel = g_default_channel;
        }
    }

    midi_send_pitchbend(channel, bend_value);
    ESP_LOGD(TAG, "apply_pitchbend ch=%d value=%d", channel, bend_value);
}

int midi_mpe_get_last_active_channel(void)
{
    if (g_last_active_row < 0) return -1;
    return (int)midi_mpe_channel_for_row(g_last_active_row);
}

void midi_mpe_lock_pitchbend_target(bool locked)
{
    g_pb_locked = locked;
    ESP_LOGD(TAG, "pb_lock set=%d", locked);
}

void midi_mpe_reset_pitchbend_target(void)
{
    g_last_active_row = -1;
    ESP_LOGD(TAG, "pb target reset");
}

void midi_mpe_set_base_channel(uint8_t base)
{
    /* Accept 0..15 (0-based channels) but typical MPE uses 1-based numbering in UI.
     * We store 0-based.
     */
    g_mpe_base_channel = base;
}

uint8_t midi_mpe_get_base_channel(void)
{
    return g_mpe_base_channel;
}

uint8_t midi_mpe_channel_for_row(int row)
{
    if (row < 0) return g_default_channel;
    /* Map row 0.. to base..base+rows-1; keep within 0..15 */
    uint32_t ch = (uint32_t)g_mpe_base_channel + (uint32_t)row;
    if (ch > 15) ch = 15;
    return (uint8_t)ch;
}

uint8_t midi_mpe_default_channel(void)
{
    return g_default_channel;
}
