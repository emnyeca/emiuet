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
#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"

static matrix_event_cb_t g_cb = NULL;
static TaskHandle_t g_scan_task = NULL;

/* per-key debounce counters and hardware-observed state */
static uint8_t key_state[MATRIX_NUM_ROWS][MATRIX_NUM_COLS];
static bool hw_pressed[MATRIX_NUM_ROWS][MATRIX_NUM_COLS];
/* simulator-provided pressed state (visible when sim_enabled) */
static bool sim_pressed[MATRIX_NUM_ROWS][MATRIX_NUM_COLS];
static bool sim_enabled = false;
/* mutex to protect access to hw_pressed/sim_pressed/effective reads */
static portMUX_TYPE s_matrix_mux = portMUX_INITIALIZER_UNLOCKED;

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
                    portENTER_CRITICAL(&s_matrix_mux);
                    hw_pressed[r][c] = pressed;
                    key_state[r][c] = pressed ? MATRIX_DEBOUNCE_COUNT : 0;
                    portEXIT_CRITICAL(&s_matrix_mux);
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

                /* Debounce transition logic: only trigger on change of hw_pressed */
                bool changed = false;
                bool cb_val = false;
                portENTER_CRITICAL(&s_matrix_mux);
                if (!hw_pressed[r][c] && key_state[r][c] >= MATRIX_DEBOUNCE_COUNT) {
                    hw_pressed[r][c] = true;
                    changed = true;
                    cb_val = true;
                } else if (hw_pressed[r][c] && key_state[r][c] == 0) {
                    hw_pressed[r][c] = false;
                    changed = true;
                    cb_val = false;
                }
                portEXIT_CRITICAL(&s_matrix_mux);

                if (changed && g_cb) {
                    g_cb(r, c, cb_val);
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
    memset(hw_pressed, 0, sizeof(hw_pressed));
    memset(sim_pressed, 0, sizeof(sim_pressed));
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

bool matrix_scan_is_pressed(int row, int col)
{
    if (row < 0 || row >= MATRIX_NUM_ROWS || col < 0 || col >= MATRIX_NUM_COLS) return false;
    bool val = false;
    portENTER_CRITICAL(&s_matrix_mux);
    if (sim_enabled) {
        val = sim_pressed[row][col];
    } else {
        val = hw_pressed[row][col];
    }
    portEXIT_CRITICAL(&s_matrix_mux);
    return val;
}

void matrix_scan_set_sim_enabled(bool en)
{
    portENTER_CRITICAL(&s_matrix_mux);
    sim_enabled = en;
    portEXIT_CRITICAL(&s_matrix_mux);
}

void matrix_scan_set_sim_state(int row, int col, bool pressed)
{
    if (row < 0 || row >= MATRIX_NUM_ROWS || col < 0 || col >= MATRIX_NUM_COLS) return;
    portENTER_CRITICAL(&s_matrix_mux);
    sim_pressed[row][col] = pressed;
    portEXIT_CRITICAL(&s_matrix_mux);
    /* invoke callback outside critical section */
    if (g_cb) g_cb(row, col, pressed);
}

/* =========================================================
 * Simulator helpers
 * - One task per string (row)
 * - Each row has its own PRNG (no rand()) -> truly independent timing/notes
 * - Supports "chords" within a row (multiple cols at once)
 * - Batch update to reduce OLED catching intermediate states
 * ========================================================= */

static TaskHandle_t s_sim_tasks[MATRIX_NUM_ROWS] = {0};

/* xorshift32: tiny per-task PRNG */
static inline uint32_t prng_next_u32(uint32_t *s)
{
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

static inline uint32_t prng_range(uint32_t *s, uint32_t lo, uint32_t hi_inclusive)
{
    uint32_t r = prng_next_u32(s);
    uint32_t span = (hi_inclusive - lo) + 1;
    return lo + (r % span);
}

/* Batch-set multiple columns in a row */
static void matrix_scan_set_sim_state_multi(int row, const int *cols, int n, bool pressed)
{
    if (row < 0 || row >= MATRIX_NUM_ROWS || n <= 0) return;

    portENTER_CRITICAL(&s_matrix_mux);
    for (int i = 0; i < n; ++i) {
        int c = cols[i];
        if (c < 0 || c >= MATRIX_NUM_COLS) continue;
        sim_pressed[row][c] = pressed;
    }
    portEXIT_CRITICAL(&s_matrix_mux);

    /* callbacks outside critical section */
    if (g_cb) {
        for (int i = 0; i < n; ++i) {
            int c = cols[i];
            if (c < 0 || c >= MATRIX_NUM_COLS) continue;
            g_cb(row, c, pressed);
        }
    }
}

/* Pick up to k unique columns for a chord within a row */
static int pick_unique_cols(uint32_t *rng, int *out_cols, int k)
{
    if (k <= 0) return 0;
    if (k > MATRIX_NUM_COLS) k = MATRIX_NUM_COLS;

    int n = 0;
    while (n < k) {
        int c = (int)prng_range(rng, 0, MATRIX_NUM_COLS - 1);

        bool dup = false;
        for (int i = 0; i < n; ++i) {
            if (out_cols[i] == c) { dup = true; break; }
        }
        if (dup) continue;

        out_cols[n++] = c;
    }
    return n;
}

static void sim_string_task(void *arg)
{
    int row = (int)(intptr_t)arg;

    /* Per-row seed: mix timer + row + esp_random if available */
    uint32_t seed = (uint32_t)esp_timer_get_time() ^ (0x9E3779B9u * (uint32_t)(row + 1));
#if __has_include("esp_random.h")
#include "esp_random.h"
    seed ^= esp_random();
#endif
    if (seed == 0) seed = 1; /* avoid zero-lock */

    /* Start offset so rows don’t line up */
    vTaskDelay(pdMS_TO_TICKS(50 + (row * 37)));

    /* Keep track of currently pressed chord to release cleanly */
    int cur_cols[4] = {0};
    int cur_n = 0;

    while (1) {
        /* Only act when sim is enabled */
        bool se = false;
        portENTER_CRITICAL(&s_matrix_mux);
        se = sim_enabled;
        portEXIT_CRITICAL(&s_matrix_mux);

        if (!se) {
            /* Ensure nothing left stuck when sim gets disabled */
            if (cur_n > 0) {
                matrix_scan_set_sim_state_multi(row, cur_cols, cur_n, false);
                cur_n = 0;
            }
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        /* Decide chord size:
         *  - mostly 1 note
         *  - sometimes 2, rarely 3
         */
        uint32_t dice = prng_range(&seed, 0, 99);
        int want = (dice < 70) ? 1 : (dice < 93) ? 2 : 3;

        int new_cols[4] = {0};
        int new_n = pick_unique_cols(&seed, new_cols, want);

        /* Timing */
        uint32_t press_ms = prng_range(&seed, 180, 1200);
        uint32_t gap_ms   = prng_range(&seed, 60,  800);

        /* Release previous chord first (clean NOTE_OFF), then press new chord.
         * This can create a “blank” moment if OLED samples between calls.
         * To minimize the chance of “all off” look:
         *  - keep gap small, and each row is desynced by independent PRNG + offsets.
         */
        if (cur_n > 0) {
            matrix_scan_set_sim_state_multi(row, cur_cols, cur_n, false);
            cur_n = 0;
        }

        matrix_scan_set_sim_state_multi(row, new_cols, new_n, true);
        for (int i = 0; i < new_n; ++i) cur_cols[i] = new_cols[i];
        cur_n = new_n;

        vTaskDelay(pdMS_TO_TICKS(press_ms));

        /* Release */
        if (cur_n > 0) {
            matrix_scan_set_sim_state_multi(row, cur_cols, cur_n, false);
            cur_n = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(gap_ms));
    }
}

void matrix_sim_start(void)
{
    for (int r = 0; r < MATRIX_NUM_ROWS; ++r) {
        if (!s_sim_tasks[r]) {
            char name[16];
            snprintf(name, sizeof(name), "sim_row_%d", r);
            xTaskCreate(sim_string_task, name, 4096, (void *)(intptr_t)r, 5, &s_sim_tasks[r]);
        }
    }
}

void matrix_sim_stop(void)
{
    for (int r = 0; r < MATRIX_NUM_ROWS; ++r) {
        if (s_sim_tasks[r]) {
            vTaskDelete(s_sim_tasks[r]);
            s_sim_tasks[r] = NULL;
        }
    }
}