#include "led_control.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "sdkconfig.h"

#ifndef CONFIG_EMIUET_RGB_GLOBAL_BRIGHTNESS_MAX
#define CONFIG_EMIUET_RGB_GLOBAL_BRIGHTNESS_MAX 96
#endif

static const uint8_t s_string_base_note[EMUIET_LED_ROWS] = {64, 59, 55, 50, 45, 40};
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static led_rgb_t s_pixels[EMUIET_LED_COUNT];
static uint16_t s_note_channels[128];
static uint8_t s_brightness = CONFIG_EMIUET_RGB_GLOBAL_BRIGHTNESS_MAX;
static led_display_mode_t s_mode = LED_DISPLAY_NOTES;
static uint32_t s_generation;

uint16_t led_layout_physical_index(uint8_t row, uint8_t col)
{
    if (row >= EMUIET_LED_ROWS || col >= EMUIET_LED_COLS) return 0;
    const uint8_t chain_col = (row & 1u) ? (EMUIET_LED_COLS - 1u - col) : col;
    return (uint16_t)(row * EMUIET_LED_COLS + chain_col);
}

static void rebuild_note_pixels(void)
{
    memset(s_pixels, 0, sizeof(s_pixels));
    if (s_mode == LED_DISPLAY_OFF) return;
    for (uint8_t row = 0; row < EMUIET_LED_ROWS; ++row) {
        for (uint8_t col = 0; col < EMUIET_LED_COLS; ++col) {
            const uint8_t note = (uint8_t)(s_string_base_note[row] + col);
            if (s_note_channels[note]) {
                const uint16_t physical = led_layout_physical_index(row, col);
                s_pixels[physical] = (led_rgb_t){.red = 32, .green = 96, .blue = 255};
            }
        }
    }
}

void led_control_init(void)
{
    portENTER_CRITICAL(&s_mux);
    memset(s_note_channels, 0, sizeof(s_note_channels));
    memset(s_pixels, 0, sizeof(s_pixels));
    s_brightness = CONFIG_EMIUET_RGB_GLOBAL_BRIGHTNESS_MAX;
    s_mode = LED_DISPLAY_NOTES;
    ++s_generation;
    portEXIT_CRITICAL(&s_mux);
}

void led_control_handle_midi(uint8_t status, uint8_t data1, uint8_t data2)
{
    const uint8_t type = status & 0xF0u;
    const uint8_t channel = status & 0x0Fu;
    data1 &= 0x7Fu;
    data2 &= 0x7Fu;
    portENTER_CRITICAL(&s_mux);
    if (type == 0x90u && data2 != 0) {
        s_note_channels[data1] |= (uint16_t)(1u << channel);
    } else if (type == 0x80u || (type == 0x90u && data2 == 0)) {
        s_note_channels[data1] &= (uint16_t)~(1u << channel);
    } else if (type == 0xB0u && data1 == 123u) {
        const uint16_t keep_mask = (uint16_t)~(1u << channel);
        for (unsigned note = 0; note < 128; ++note) s_note_channels[note] &= keep_mask;
    } else {
        portEXIT_CRITICAL(&s_mux);
        return;
    }
    rebuild_note_pixels();
    ++s_generation;
    portEXIT_CRITICAL(&s_mux);
}

void led_control_set_global_brightness(uint8_t brightness)
{
    if (brightness > CONFIG_EMIUET_RGB_GLOBAL_BRIGHTNESS_MAX) {
        brightness = CONFIG_EMIUET_RGB_GLOBAL_BRIGHTNESS_MAX;
    }
    portENTER_CRITICAL(&s_mux);
    s_brightness = brightness;
    ++s_generation;
    portEXIT_CRITICAL(&s_mux);
}

void led_control_set_display_mode(led_display_mode_t mode)
{
    portENTER_CRITICAL(&s_mux);
    s_mode = mode;
    rebuild_note_pixels();
    ++s_generation;
    portEXIT_CRITICAL(&s_mux);
}

uint32_t led_control_snapshot(led_rgb_t pixels[EMUIET_LED_COUNT], uint8_t *brightness)
{
    uint32_t generation;
    portENTER_CRITICAL(&s_mux);
    memcpy(pixels, s_pixels, sizeof(s_pixels));
    *brightness = s_brightness;
    generation = s_generation;
    portEXIT_CRITICAL(&s_mux);
    return generation;
}
