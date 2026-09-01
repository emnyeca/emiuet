#pragma once

#include <stdbool.h>
#include <stdint.h>

#define EMUIET_LED_ROWS 6
#define EMUIET_LED_COLS 13
#define EMUIET_LED_COUNT (EMUIET_LED_ROWS * EMUIET_LED_COLS)

typedef struct { uint8_t red, green, blue; } led_rgb_t;

typedef enum {
    LED_DISPLAY_NOTES = 0,
    LED_DISPLAY_OFF,
} led_display_mode_t;

void led_control_init(void);
void led_control_handle_midi(uint8_t status, uint8_t data1, uint8_t data2);
void led_control_set_global_brightness(uint8_t brightness);
void led_control_set_display_mode(led_display_mode_t mode);
uint32_t led_control_snapshot(led_rgb_t pixels[EMUIET_LED_COUNT], uint8_t *brightness);
uint16_t led_layout_physical_index(uint8_t row, uint8_t col);
