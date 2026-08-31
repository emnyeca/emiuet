#pragma once

/* Compatibility API. Input now routes to MIDI or USB HID by explicit mode. */
void matrix_midi_bridge_start(int discard_cycles);
void matrix_midi_bridge_stop(void);
