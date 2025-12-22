#pragma once

#include <stdbool.h>
#include <stdint.h>

/* =========================================================
 * MIDI output API (single exit point)
 *
 * Goals:
 * - Keep higher-level musical logic independent of transport (USB/BLE/TRS).
 * - Allow multi-route output via a bitmask without touching callers.
 * - Preserve existing midi_send_* APIs as compatibility wrappers.
 * ========================================================= */

typedef enum {
	MIDI_OUT_ROUTE_USB = 1u << 0,
	MIDI_OUT_ROUTE_TRS_UART = 1u << 1,
	MIDI_OUT_ROUTE_BLE = 1u << 2,
} midi_out_routes_t;

typedef struct {
	uint32_t routes; /* bitmask of midi_out_routes_t */
} midi_out_config_t;

typedef enum {
	MIDI_MSG_NOTE_ON = 0,
	MIDI_MSG_NOTE_OFF,
	MIDI_MSG_CC,
	MIDI_MSG_PITCH_BEND,
	MIDI_MSG_CH_PRESSURE,
	MIDI_MSG_PROGRAM_CHANGE,
} midi_msg_type_t;

typedef struct {
	midi_msg_type_t type;
	uint8_t channel; /* 0..15 */
	union {
		struct {
			uint8_t note;
			uint8_t velocity;
		} note;
		struct {
			uint8_t cc;
			uint8_t value;
		} cc;
		struct {
			uint16_t value; /* 0..16383 (14-bit), 8192 == center */
		} pitchbend;
		struct {
			uint8_t value; /* 0..127 */
		} ch_pressure;
		struct {
			uint8_t program; /* 0..127 */
		} program;
	} data;
} midi_msg_t;

/* Initialize midi output subsystem with defaults (compat entrypoint). */
void midi_out_init(void);

/* Initialize midi output subsystem.
 * If cfg is NULL, defaults are used.
 */
void midi_out_init_ex(const midi_out_config_t *cfg);

/* Set active output routes (bitmask of midi_out_routes_t). */
void midi_out_set_routes(uint32_t routes);
uint32_t midi_out_get_routes(void);

/* Send a structured MIDI message to all active routes. */
bool midi_out_send(const midi_msg_t *msg);

/* =========================================================
 * Compatibility wrappers (existing call sites)
 * ========================================================= */
void midi_send_note_on(uint8_t channel, uint8_t note, uint8_t velocity);
void midi_send_note_off(uint8_t channel, uint8_t note, uint8_t velocity);
void midi_send_cc(uint8_t channel, uint8_t cc, uint8_t value);
void midi_send_pitchbend(uint8_t channel, uint16_t value);
void midi_send_ch_pressure(uint8_t channel, uint8_t value);
void midi_send_program_change(uint8_t channel, uint8_t program);
