#include "input_router.h"

#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"

#include "board_pins.h"
#include "keyboard_input.h"
#include "keyboard_keymap.h"
#include "matrix_scan.h"
#include "midi_mpe.h"
#include "midi_out.h"

typedef struct {
    bool active;
    uint8_t channel;
    uint8_t note;
} active_midi_key_t;

static const char *TAG = "input_router";
static const uint8_t s_string_base_note[MATRIX_NUM_ROWS] = {64, 59, 55, 50, 45, 40};
static const int s_mode_chord[4][2] = {{0, 0}, {0, 12}, {5, 0}, {5, 12}};

static volatile input_mode_t s_mode = INPUT_MODE_MIDI;
static active_midi_key_t s_active_midi[MATRIX_NUM_ROWS][MATRIX_NUM_COLS];
static bool s_matrix_pressed[MATRIX_NUM_ROWS][MATRIX_NUM_COLS];
static esp_timer_handle_t s_mode_timer = NULL;
static bool s_mode_chord_latched = false;
static SemaphoreHandle_t s_router_lock = NULL;
static bool s_started = false;

static bool mode_chord_is_pressed(void)
{
    for (size_t i = 0; i < 4; ++i) {
        if (!s_matrix_pressed[s_mode_chord[i][0]][s_mode_chord[i][1]]) return false;
    }
    return true;
}

static void release_midi_state(void)
{
    for (int row = 0; row < MATRIX_NUM_ROWS; ++row) {
        for (int col = 0; col < MATRIX_NUM_COLS; ++col) {
            active_midi_key_t *active = &s_active_midi[row][col];
            if (active->active) {
                midi_send_note_off(active->channel, active->note, 0);
                active->active = false;
            }
        }
    }

    /* Belt-and-suspenders reset for receivers after an interrupted transition. */
    for (uint8_t channel = 0; channel < 16; ++channel) {
        midi_send_cc(channel, 123, 0); /* All Notes Off */
        midi_send_pitchbend(channel, 8192);
    }
}

void input_router_set_mode(input_mode_t mode)
{
    if (mode != INPUT_MODE_MIDI && mode != INPUT_MODE_KEYBOARD) return;

    if (s_router_lock) (void)xSemaphoreTake(s_router_lock, portMAX_DELAY);
    input_mode_t previous = s_mode;
    if (previous == mode) {
        if (s_router_lock) (void)xSemaphoreGive(s_router_lock);
        return;
    }
    s_mode = mode;

    if (previous == INPUT_MODE_MIDI) {
        release_midi_state();
    } else {
        keyboard_input_release_all();
    }

    if (s_router_lock) (void)xSemaphoreGive(s_router_lock);

    ESP_LOGI(TAG, "input mode: %s", mode == INPUT_MODE_MIDI ? "MIDI" : "TYPE");
}

static void mode_timer_cb(void *arg)
{
    (void)arg;
    input_mode_t target = INPUT_MODE_MIDI;

    if (s_router_lock) (void)xSemaphoreTake(s_router_lock, portMAX_DELAY);
    if (!mode_chord_is_pressed() || s_mode_chord_latched) {
        if (s_router_lock) (void)xSemaphoreGive(s_router_lock);
        return;
    }

    s_mode_chord_latched = true;
    target = s_mode == INPUT_MODE_MIDI ? INPUT_MODE_KEYBOARD : INPUT_MODE_MIDI;
    if (s_router_lock) (void)xSemaphoreGive(s_router_lock);
    input_router_set_mode(target);
}

static void update_mode_chord_timer(void)
{
    if (!s_mode_timer) return;

    if (mode_chord_is_pressed()) {
        if (!s_mode_chord_latched && !esp_timer_is_active(s_mode_timer)) {
            (void)esp_timer_start_once(s_mode_timer, INPUT_MODE_CHORD_HOLD_MS * 1000ULL);
        }
    } else {
        if (esp_timer_is_active(s_mode_timer)) (void)esp_timer_stop(s_mode_timer);
        s_mode_chord_latched = false;
    }
}

static void handle_midi_event(int row, int col, bool pressed)
{
    active_midi_key_t *active = &s_active_midi[row][col];

    if (pressed) {
        if (active->active) return;
        const uint8_t channel = midi_mpe_is_enabled()
                                    ? midi_mpe_channel_for_row(row)
                                    : midi_mpe_default_channel();
        const uint8_t note = (uint8_t)(s_string_base_note[row] + col);
        active->active = true;
        active->channel = channel;
        active->note = note;
        midi_mpe_note_activity(row);
        midi_send_note_on(channel, note, 100);
    } else if (active->active) {
        /* Use the channel captured on Note On even if MPE changed meanwhile. */
        midi_send_note_off(active->channel, active->note, 0);
        active->active = false;
    }
}

static void on_matrix_event(int row, int col, bool pressed)
{
    if (row < 0 || row >= MATRIX_NUM_ROWS || col < 0 || col >= MATRIX_NUM_COLS) return;

    if (s_router_lock) (void)xSemaphoreTake(s_router_lock, portMAX_DELAY);
    s_matrix_pressed[row][col] = pressed;
    update_mode_chord_timer();

    if (s_mode == INPUT_MODE_MIDI) {
        handle_midi_event(row, col, pressed);
    } else if (!keyboard_keymap_is_mode_chord_key(row, col)) {
        keyboard_input_handle_event(row, col, pressed);
    }
    if (s_router_lock) (void)xSemaphoreGive(s_router_lock);
}

void input_router_start(int discard_cycles)
{
    if (!s_router_lock) s_router_lock = xSemaphoreCreateMutex();
    midi_out_init();
    midi_mpe_init();
    s_started = true;

    if (!s_mode_timer) {
        const esp_timer_create_args_t timer_args = {
            .callback = mode_timer_cb,
            .name = "input_mode",
        };
        if (esp_timer_create(&timer_args, &s_mode_timer) != ESP_OK) {
            ESP_LOGE(TAG, "failed to create input mode timer");
        }
    }

    matrix_scan_start(on_matrix_event, discard_cycles);

#if CONFIG_MATRIX_SIM_ENABLED_DEFAULT
    matrix_scan_set_sim_enabled(true);
    matrix_sim_start();
    ESP_LOGW(TAG, "Matrix simulator ENABLED (CONFIG_MATRIX_SIM_ENABLED_DEFAULT=y)");
#else
    matrix_scan_set_sim_enabled(false);
#endif

    ESP_LOGI(TAG, "input router started in MIDI mode (discard_cycles=%d)", discard_cycles);
}

void input_router_stop(void)
{
    if (s_router_lock) (void)xSemaphoreTake(s_router_lock, portMAX_DELAY);
    release_midi_state();
    keyboard_input_release_all();
    if (s_router_lock) (void)xSemaphoreGive(s_router_lock);
    matrix_scan_stop();
    s_started = false;
}

input_mode_t input_router_get_mode(void)
{
    return s_mode;
}

bool input_router_is_midi_mode(void)
{
    return input_router_get_mode() == INPUT_MODE_MIDI;
}

void input_router_usb_disconnected(void)
{
    /* HID state is invalid after detach; MIDI releases still reach TRS/BLE routes. */
    if (!s_started) return;
    if (s_router_lock) (void)xSemaphoreTake(s_router_lock, portMAX_DELAY);
    keyboard_input_release_all();
    release_midi_state();
    if (s_router_lock) (void)xSemaphoreGive(s_router_lock);
}
