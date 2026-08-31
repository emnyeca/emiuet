#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "input_mode.h"

#define INPUT_MODE_CHORD_HOLD_MS 2000

void input_router_start(int discard_cycles);
void input_router_stop(void);
input_mode_t input_router_get_mode(void);
bool input_router_is_midi_mode(void);
void input_router_set_mode(input_mode_t mode);

/* Called by the USB backend on detach to clear transport-independent state. */
void input_router_usb_disconnected(void);

