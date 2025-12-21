#pragma once

#include <stdint.h>

/* Minimal MIDI output abstraction.
 * Implementers should replace these stubs with USB/BLE/TRS MIDI senders.
 */

void midi_out_init(void);
void midi_send_note_on(uint8_t channel, uint8_t note, uint8_t velocity);
void midi_send_note_off(uint8_t channel, uint8_t note, uint8_t velocity);
void midi_send_pitchbend(uint8_t channel, uint16_t value);
