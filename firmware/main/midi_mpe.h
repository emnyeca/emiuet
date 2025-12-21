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
 * - base channel is the lowest MIDI channel used for MPE string mapping
 * - e.g., base=1 maps rows 0..5 to channels base..base+5
 */
void midi_mpe_set_base_channel(uint8_t base);
uint8_t midi_mpe_get_base_channel(void);
uint8_t midi_mpe_channel_for_row(int row);
uint8_t midi_mpe_default_channel(void);
