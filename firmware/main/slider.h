#pragma once

#include <stdint.h>
#include <stdbool.h>

/* Slider driver
 * - Reads ADC for pitch/mod/vel sliders
 * - Exposes normalized values (0..1023)
 * - Pitch bend behavior (upward-only) handled in midi_mpe layer
 */

void slider_init(void);
uint16_t slider_read_pitchbend(void);
uint16_t slider_read_mod(void);
uint16_t slider_read_velocity(void);

/* Start a background task that polls the pitch-bend slider and applies
 * pitch-bend messages via `midi_mpe_apply_pitchbend`. Call after midi/init.
 */
void slider_task_start(void);

/* Return true if slider ADC is available and initialized */
bool slider_is_enabled(void);
