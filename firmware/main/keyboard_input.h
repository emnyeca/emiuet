#pragma once

#include <stdbool.h>
#include <stdint.h>

void keyboard_input_handle_event(int row, int col, bool pressed);
void keyboard_input_release_all(void);
uint8_t keyboard_input_get_layer(void);

