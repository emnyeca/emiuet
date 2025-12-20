#pragma once

/*
 * Emiuet Board Pin Definitions
 * Source of Truth: docs/pinout-v3.md
 * Board: ESP32-S3-MINI-1 (Emiuet PCB Rev.3)
 *
 * IMPORTANT:
 * - Do not hardcode GPIO numbers elsewhere.
 * - Always include this header for pin references.
 * - Strapping pins (GPIO45 / GPIO46) require delayed scan after boot.
 */

#include <stdint.h>
#include "driver/gpio.h"

/* IntelliSense / tooling fallback:
 * Some VS Code setups don't pick up ESP-IDF gpio enums correctly.
 * These guards keep editor diagnostics sane without affecting real builds.
 */
#ifndef GPIO_NUM_45
#define GPIO_NUM_45 ((gpio_num_t)45)
#endif
#ifndef GPIO_NUM_46
#define GPIO_NUM_46 ((gpio_num_t)46)
#endif
#ifndef GPIO_NUM_47
#define GPIO_NUM_47 ((gpio_num_t)47)
#endif
#ifndef GPIO_NUM_48
#define GPIO_NUM_48 ((gpio_num_t)48)
#endif

/* =========================================================
 * System / Communication / Power
 * ========================================================= */

#define PIN_USB_D_MINUS        GPIO_NUM_19
#define PIN_USB_D_PLUS         GPIO_NUM_20

#define PIN_MIDI_OUT_TX        GPIO_NUM_43   /* UART0 TX, TRS Type-A */
#define PIN_UART0_RX_SHARED    GPIO_NUM_44   /* Shared with SW_LEFT */

#define PIN_I2C_SDA            GPIO_NUM_18
#define PIN_I2C_SCL            GPIO_NUM_16

#define PIN_BAT_VSENSE         GPIO_NUM_17   /* ADC2_CH6 */

#define PIN_CHG_STATUS         GPIO_NUM_48   /* External pull-up */
#define PIN_PGOOD_STATUS       GPIO_NUM_38   /* External pull-up */

/* =========================================================
 * Analog Inputs (Sliders)
 * ========================================================= */

#define PIN_SLIDER_PB          GPIO_NUM_1    /* Pitch Bend (upward only) */
#define PIN_SLIDER_MOD         GPIO_NUM_2    /* CC#1 */
#define PIN_SLIDER_VEL         GPIO_NUM_4    /* Velocity (NoteOn sampled) */

/* =========================================================
 * UI Elements
 * ========================================================= */

#define PIN_STATUS_LED         GPIO_NUM_6

#define PIN_SW_CENTER          GPIO_NUM_40   /* MPE toggle / BLE pairing */
#define PIN_SW_RIGHT           GPIO_NUM_39   /* Octave Up */
#define PIN_SW_LEFT            GPIO_NUM_44   /* Octave Down (UART RX shared) */

/* =========================================================
 * Key Matrix (6 Rows x 13 Columns)
 * ========================================================= */

#define MATRIX_NUM_ROWS 6
#define MATRIX_NUM_COLS 13

/* Row drive pins (Strings) */
extern const gpio_num_t MATRIX_ROW_PINS[MATRIX_NUM_ROWS];

/* Column sense pins (Frets)
 * NOTE: GPIO45 / GPIO46 are strapping pins.
 * Do NOT start matrix scanning immediately after boot.
 */
extern const gpio_num_t MATRIX_COL_PINS[MATRIX_NUM_COLS];

#define IS_STRAPPING_COL(col_index) ((col_index) == 0 || (col_index) == 1)

/* Delay after boot before enabling matrix scan (strapping safety) */
#define MATRIX_SCAN_START_DELAY_MS 300

/* =========================================================
 * Initialization helpers
 * =========================================================
 * Two-stage init:
 * - Early: safe pins only (LED, buttons, power status, etc.)
 * - Late: matrix pins (after boot delay) to avoid strapping issues.
 */
void board_pins_init_early(void);
void board_pins_init_matrix_late(void);
