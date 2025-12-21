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
