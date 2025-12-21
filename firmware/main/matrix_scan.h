#pragma once

#include <stdint.h>
#include <stdbool.h>

/* Non-blocking matrix scanner API
 * - 6 rows x 13 cols
 * - Deferred start (board_late_init_task) must be called before starting
 * - Caller may register an event callback to receive press/release events
 */

#define MATRIX_DEBOUNCE_MS 5
/* Number of consecutive stable reads required for state change */
#define MATRIX_DEBOUNCE_COUNT 3

typedef void (*matrix_event_cb_t)(int row, int col, bool pressed);

/* Start scanning. discard_cycles: number of full matrix cycles to ignore after start
 * (used to avoid acting on strapping-pin states during boot). */
void matrix_scan_start(matrix_event_cb_t cb, int discard_cycles);
void matrix_scan_stop(void);

/* Query the current stable pressed state for a key. Returns true if pressed. */
bool matrix_scan_is_pressed(int row, int col);

/* Simulator control: enable/disable simulated presses. When enabled,
 * `matrix_scan_is_pressed()` returns simulated state instead of hardware.
 */
void matrix_scan_set_sim_enabled(bool en);
/* Set simulated pressed state for a key (visible when sim is enabled).
 * This also invokes the registered event callback so other modules
 * (MIDI/OLED) observe the simulated press/release.
 */
void matrix_scan_set_sim_state(int row, int col, bool pressed);

/* Start/stop simulator helper that creates per-string simulation tasks.
 * These are convenience helpers used by the debug bridge.
 */
void matrix_sim_start(void);
void matrix_sim_stop(void);
