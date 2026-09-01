#pragma once

/* Emiuet Rev.B GPIO source of truth.
 * Keep GPIO0/3/45/46 free of normal loads because they are strapping pins.
 * GPIO19/20 are dedicated to native USB. GPIO26 is unavailable on N4R2.
 */

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"

#define PIN_USB_D_MINUS       GPIO_NUM_19
#define PIN_USB_D_PLUS        GPIO_NUM_20

#define PIN_I2C_SDA           GPIO_NUM_39
#define PIN_I2C_SCL           GPIO_NUM_40
#define PIN_TUSB320_INT_N     GPIO_NUM_37

#define PIN_MIDI_OUT_TX       GPIO_NUM_43
#define PIN_MIDI_IN_RX        GPIO_NUM_44

#define PIN_RGB_DATA          GPIO_NUM_38
#define PIN_STATUS_LED        GPIO_NUM_48

#define PIN_SLIDER_PB         GPIO_NUM_1
#define PIN_SLIDER_MOD        GPIO_NUM_2
#define PIN_SLIDER_VEL        GPIO_NUM_4

#define PIN_SW_CENTER         GPIO_NUM_41
#define PIN_SW_RIGHT          GPIO_NUM_42
#define PIN_SW_LEFT           GPIO_NUM_47

#define MATRIX_NUM_ROWS 6
#define MATRIX_NUM_COLS 13

extern const gpio_num_t MATRIX_ROW_PINS[MATRIX_NUM_ROWS];
extern const gpio_num_t MATRIX_COL_PINS[MATRIX_NUM_COLS];

/* No matrix signal is on a strapping pin in Rev.B. The short delay still lets
 * USB power and peripherals settle before the 78-key scanner starts.
 */
#define MATRIX_SCAN_START_DELAY_MS 100
#define IS_STRAPPING_COL(col_index) (false)

void board_pins_init_early(void);
void board_pins_init_matrix_prepare(void);
void board_pins_enable_matrix_columns(void);
void board_pins_init_matrix_late(void);

#ifndef MATRIX_INITIAL_DISCARD_CYCLES
#define MATRIX_INITIAL_DISCARD_CYCLES 5
#endif

#ifndef MATRIX_ROW_INTERNAL_PULLUP
#define MATRIX_ROW_INTERNAL_PULLUP 1
#endif
