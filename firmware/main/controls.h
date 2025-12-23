#pragma once

#include <stdint.h>

/* Centralized physical controls (buttons) state/task.
 * Owns PIN_SW_CENTER / PIN_SW_LEFT / PIN_SW_RIGHT.
 */

void controls_start(void);

/* Current octave offset in octaves, clamped to [-3, +3].
 * Intended for note calculation and OLED display.
 */
int8_t controls_get_octave(void);
