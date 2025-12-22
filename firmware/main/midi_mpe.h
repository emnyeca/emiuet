#pragma once

#include <stdint.h>
#include <stdbool.h>

/* Minimal MIDI/MPE helpers
 * - Provides MPE toggle and per-string channel mapping
 * - Provides a function to apply an upward-only pitch bend to the active string
 */

void midi_mpe_init(void);
void midi_mpe_set_enabled(bool en);
bool midi_mpe_is_enabled(void);

/* Apply pitch bend value (0..16383) to currently active string/channel */
void midi_mpe_apply_pitchbend(uint16_t bend_value);

/* Register which string (row) was last active (0..5) */
void midi_mpe_note_activity(int row);

/* MPE base channel management
 * - API is 1-based to match MIDI UI conventions (1..16)
 * - base channel is the lowest MIDI channel used for per-string mapping
 * - in MPE mode, 6 strings map to channels base..base+5
 * - e.g., base=2 maps rows 0..5 to channels 2..7 (MPE-compatible member channels)
 * - base is clamped so base+5 never exceeds channel 16
 */
void midi_mpe_set_base_channel(uint8_t base_ch1_16);
uint8_t midi_mpe_get_base_channel(void);

/* Internal channel mapping helpers (0-based for MIDI encoding):
 * - midi_mpe_channel_for_row(): returns 0..15 (0 == MIDI ch1)
 * - midi_mpe_default_channel(): returns 0..15 (0 == MIDI ch1)
 */
uint8_t midi_mpe_channel_for_row(int row);
uint8_t midi_mpe_default_channel(void);

/* Additional helpers for PB target management */
/* Return last-active channel (0..15), or -1 if none */
int midi_mpe_get_last_active_channel(void);

/* Lock/unlock pitch-bend target. When locked, note activity will not
 * change the PB target (useful while a bend is in progress). */
void midi_mpe_lock_pitchbend_target(bool locked);

/* Reset the PB target so the next PB will pick the then-last-active string */
void midi_mpe_reset_pitchbend_target(void);
