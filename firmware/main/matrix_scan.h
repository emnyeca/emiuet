#pragma once

#include <stdint.h>
#include <stdbool.h>

/* Non-blocking matrix scanner API
 * - 6 rows x 13 cols
 * - Deferred start (board_late_init_task) must be called before starting
 * - Caller may register an event callback to receive press/release events
 */

#define MATRIX_DEBOUNCE_MS 5

typedef void (*matrix_event_cb_t)(int row, int col, bool pressed);

void matrix_scan_start(matrix_event_cb_t cb);
void matrix_scan_stop(void);
