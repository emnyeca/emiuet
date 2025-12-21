#include "matrix_scan.h"
#include "board_pins.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

/* esp_rom_delay_us is provided by ROM; include availability varies across
 * ESP-IDF versions. Provide an extern declaration to avoid header-location
 * issues at compile time.
 */
extern void esp_rom_delay_us(uint32_t us);
#include <string.h>

static matrix_event_cb_t g_cb = NULL;
static TaskHandle_t g_scan_task = NULL;

/* per-key debounce counters and stable state */
static uint8_t key_state[MATRIX_NUM_ROWS][MATRIX_NUM_COLS];
static bool stable_pressed[MATRIX_NUM_ROWS][MATRIX_NUM_COLS];

/* Number of full matrix cycles to discard after start */
static int g_discard_cycles = 0;
static bool g_capture_after_discard = false;

static void select_row(int row)
{
    /* Rows default HIGH (inactive). Drive low to select. */
    for (int r = 0; r < MATRIX_NUM_ROWS; ++r) {
        gpio_set_level(MATRIX_ROW_PINS[r], (r == row) ? 0 : 1);
    }
}

static void deselect_rows(void)
{
    for (int r = 0; r < MATRIX_NUM_ROWS; ++r) {
        gpio_set_level(MATRIX_ROW_PINS[r], 1);
    }
}

static void scan_task(void *arg)
{
    (void)arg;
    TickType_t delay = pdMS_TO_TICKS(MATRIX_DEBOUNCE_MS);
    if (delay == 0) delay = 1;

    while (1) {
        /* If requested, perform a capture pass immediately after discard to
         * adopt the current physical state as initial stable_pressed values.
         */
        if (g_capture_after_discard) {
            for (int r = 0; r < MATRIX_NUM_ROWS; ++r) {
                select_row(r);
                esp_rom_delay_us(50);
                for (int c = 0; c < MATRIX_NUM_COLS; ++c) {
                    int level = gpio_get_level(MATRIX_COL_PINS[c]);
                    bool pressed = (level == 0);
                    stable_pressed[r][c] = pressed;
                    key_state[r][c] = pressed ? MATRIX_DEBOUNCE_COUNT : 0;
                }
                deselect_rows();
            }
            g_capture_after_discard = false;
        }

        for (int r = 0; r < MATRIX_NUM_ROWS; ++r) {
            select_row(r);
            /* small settle */
            esp_rom_delay_us(50);

            for (int c = 0; c < MATRIX_NUM_COLS; ++c) {
                int level = gpio_get_level(MATRIX_COL_PINS[c]);
                /* Assuming active-low read on columns when key pressed */
                bool pressed = (level == 0);

                /* If we are still in discard period, skip counter updates entirely */
                if (g_discard_cycles > 0) {
                    continue;
                }

                /* update counter with saturation */
                if (pressed) {
                    if (key_state[r][c] < 0xFE) key_state[r][c]++;
                } else {
                    if (key_state[r][c] > 0) key_state[r][c]--;
                }

                /* Debounce transition logic: only trigger on change of stable_pressed */
                if (!stable_pressed[r][c] && key_state[r][c] >= MATRIX_DEBOUNCE_COUNT) {
                    stable_pressed[r][c] = true;
                    if (g_cb) g_cb(r, c, true);
                } else if (stable_pressed[r][c] && key_state[r][c] == 0) {
                    stable_pressed[r][c] = false;
                    if (g_cb) g_cb(r, c, false);
                }
            }

            deselect_rows();
            taskYIELD();
        }

        /* completed one full matrix cycle; if discarding, decrement counter */
        if (g_discard_cycles > 0) {
            g_discard_cycles--;
            if (g_discard_cycles == 0) {
                /* request to capture current physical state on next loop */
                g_capture_after_discard = true;
            }
        }

        vTaskDelay(delay);
    }
}

void matrix_scan_start(matrix_event_cb_t cb, int discard_cycles)
{
    if (g_scan_task) return;
    g_cb = cb;
    memset(key_state, 0, sizeof(key_state));
    memset(stable_pressed, 0, sizeof(stable_pressed));
    g_discard_cycles = (discard_cycles > 0) ? discard_cycles : 0;
    xTaskCreate(scan_task, "matrix_scan", 4096, NULL, 10, &g_scan_task);
}

void matrix_scan_stop(void)
{
    if (!g_scan_task) return;
    vTaskDelete(g_scan_task);
    g_scan_task = NULL;
    g_cb = NULL;
}
