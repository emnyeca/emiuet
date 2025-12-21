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

/* Simple debounce state */
static uint8_t key_state[MATRIX_NUM_ROWS][MATRIX_NUM_COLS];

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
    const TickType_t delay = pdMS_TO_TICKS(MATRIX_DEBOUNCE_MS);

    while (1) {
        for (int r = 0; r < MATRIX_NUM_ROWS; ++r) {
            select_row(r);
            /* small settle */
            esp_rom_delay_us(50);

            for (int c = 0; c < MATRIX_NUM_COLS; ++c) {
                int level = gpio_get_level(MATRIX_COL_PINS[c]);
                /* Assuming active-low read on columns when key pressed */
                bool pressed = (level == 0);

                if (pressed) {
                    if (key_state[r][c] < 0xFF) key_state[r][c]++;
                } else {
                    if (key_state[r][c] > 0) key_state[r][c]--;
                }

                if (key_state[r][c] == 3 && g_cb) {
                    g_cb(r, c, true);
                } else if (key_state[r][c] == 2 && !pressed && g_cb) {
                    g_cb(r, c, false);
                }
            }

            deselect_rows();
        }

        vTaskDelay(delay);
    }
}

void matrix_scan_start(matrix_event_cb_t cb)
{
    if (g_scan_task) return;
    g_cb = cb;
    memset(key_state, 0, sizeof(key_state));
    xTaskCreate(scan_task, "matrix_scan", 4096, NULL, 10, &g_scan_task);
}

void matrix_scan_stop(void)
{
    if (!g_scan_task) return;
    vTaskDelete(g_scan_task);
    g_scan_task = NULL;
    g_cb = NULL;
}
