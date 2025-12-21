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
