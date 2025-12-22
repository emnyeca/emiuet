
#include "midi_mpe.h"
#include "midi_out.h"
#include <stdbool.h>
#include "esp_log.h"

static const char *TAG = "midi_mpe";

#define MPE_NUM_STRINGS 6

static bool g_mpe_enabled = false;
static int g_last_active_row = -1; /* -1 == no last-active row (reset state) */
/* Internal: 0-based MIDI channel used as the base for per-string mapping.
 * 0 == MIDI channel 1.
 * Default is 1 (i.e., MIDI channel 2) to match common MPE member-channel layout.
 */
static uint8_t g_mpe_base_channel_ch0 = 1;

/* When true, note activity will NOT update g_last_active_row. This is used
 * to lock the pitch-bend target while a bend is in progress. */
static bool g_pb_locked = false;

/* Internal default non-MPE channel (0-based): 0 == MIDI channel 1 */
static const uint8_t g_default_channel_ch0 = 0;

static inline uint8_t clamp_ch1_16(uint8_t ch1_16)
{
    if (ch1_16 < 1) return 1;
    if (ch1_16 > 16) return 16;
    return ch1_16;
}

static inline uint8_t ch1_to_ch0(uint8_t ch1_16)
{
    return (uint8_t)(clamp_ch1_16(ch1_16) - 1);
}

static inline uint8_t ch0_to_ch1(uint8_t ch0_15)
{
    if (ch0_15 > 15) ch0_15 = 15;
    return (uint8_t)(ch0_15 + 1);
}

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
    if (!g_pb_locked && row >= 0 && row < MPE_NUM_STRINGS) g_last_active_row = row;
}

/* bend_value: 0..16383 (14-bit). Interpretation: 8192 == center.
 * This device uses upward-only bends: callers should map controller input
 * such that 0 -> center (8192) and max -> 16383.
 */
void midi_mpe_apply_pitchbend(uint16_t bend_value)
{
    uint8_t channel_ch0 = g_default_channel_ch0;
    if (g_mpe_enabled) {
        if (g_last_active_row >= 0) {
            channel_ch0 = midi_mpe_channel_for_row(g_last_active_row);
        } else {
            /* No last-active string selected: fall back to default channel */
            channel_ch0 = g_default_channel_ch0;
        }
    }

    midi_send_pitchbend(channel_ch0, bend_value);
    ESP_LOGD(TAG, "apply_pitchbend ch0=%d(ch%d) value=%d", (int)channel_ch0, (int)ch0_to_ch1(channel_ch0), (int)bend_value);
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

void midi_mpe_set_base_channel(uint8_t base_ch1_16)
{
    /* Public API is 1-based (1..16) to match MIDI UI conventions.
     * Internally we keep 0-based channels for encoding.
     * In MPE mode we use 6 channels (one per string): base..base+5.
     * Clamp base so that base+5 never exceeds MIDI channel 16.
     */
    const uint8_t max_base_ch1_16 = (uint8_t)(16 - (MPE_NUM_STRINGS - 1)); /* 11 */
    if (base_ch1_16 < 1) base_ch1_16 = 1;
    if (base_ch1_16 > max_base_ch1_16) base_ch1_16 = max_base_ch1_16;
    g_mpe_base_channel_ch0 = (uint8_t)(base_ch1_16 - 1);
}

uint8_t midi_mpe_get_base_channel(void)
{
    return ch0_to_ch1(g_mpe_base_channel_ch0);
}

uint8_t midi_mpe_channel_for_row(int row)
{
    if (row < 0) return g_default_channel_ch0;
    /* Map row 0.. to base..base+rows-1; keep within 0..15 */
    uint32_t ch = (uint32_t)g_mpe_base_channel_ch0 + (uint32_t)row;
    if (ch > 15) ch = 15;
    return (uint8_t)ch;
}

uint8_t midi_mpe_default_channel(void)
{
    return g_default_channel_ch0;
}
