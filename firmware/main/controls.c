#include "controls.h"

#include "board_pins.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "midi_mpe.h"

static const char *TAG = "controls";

#ifndef CONTROLS_POLL_MS
#define CONTROLS_POLL_MS 10
#endif

/* Debounce: accept a change only after it is stable this long. */
#ifndef CONTROLS_DEBOUNCE_US
#define CONTROLS_DEBOUNCE_US 30000
#endif

/* Long-press threshold for SW_CENTER. */
#ifndef CONTROLS_CENTER_LONGPRESS_US
#define CONTROLS_CENTER_LONGPRESS_US 800000
#endif

typedef struct {
    int raw_level;               /* last sampled level */
    int stable_level;            /* debounced stable level */
    int64_t last_raw_change_us;  /* timestamp of last raw change */

    /* Press tracking for actions */
    bool pressed;                /* stable pressed state */
    int64_t press_start_us;      /* when stable press began */
    bool long_fired;             /* long press already fired */
} button_state_t;

static TaskHandle_t s_task = NULL;
static int8_t s_octave = 0;

/* Stub hook: implement this symbol elsewhere when BLE pairing is real. */
__attribute__((weak)) void emiuet_ble_pairing_request(void)
{
    ESP_LOGW(TAG, "BLE pairing requested (stub)");
}

static inline int clamp_i8(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void button_init(button_state_t *b)
{
    int level = 1;
    b->raw_level = level;
    b->stable_level = level;
    b->last_raw_change_us = esp_timer_get_time();
    b->pressed = false;
    b->press_start_us = 0;
    b->long_fired = false;
}

static bool button_update(button_state_t *b, gpio_num_t pin, int64_t now_us)
{
    int level = gpio_get_level(pin);
    if (level != b->raw_level) {
        b->raw_level = level;
        b->last_raw_change_us = now_us;
    }

    if (b->stable_level != b->raw_level) {
        if ((now_us - b->last_raw_change_us) >= CONTROLS_DEBOUNCE_US) {
            b->stable_level = b->raw_level;
            return true; /* stable edge */
        }
    }

    return false;
}

static inline bool is_pressed_level(int stable_level)
{
    /* Active-low buttons */
    return stable_level == 0;
}

static void controls_task(void *arg)
{
    (void)arg;

    TickType_t delay_ticks = pdMS_TO_TICKS(CONTROLS_POLL_MS);
    if (delay_ticks < 1) delay_ticks = 1;

    button_state_t b_center;
    button_state_t b_left;
    button_state_t b_right;

    button_init(&b_center);
    button_init(&b_left);
    button_init(&b_right);

    /* Seed initial stable levels from pins (after board_pins_init_early). */
    int64_t now_us = esp_timer_get_time();
    b_center.raw_level = b_center.stable_level = gpio_get_level(PIN_SW_CENTER);
    b_left.raw_level = b_left.stable_level = gpio_get_level(PIN_SW_LEFT);
    b_right.raw_level = b_right.stable_level = gpio_get_level(PIN_SW_RIGHT);
    b_center.last_raw_change_us = b_left.last_raw_change_us = b_right.last_raw_change_us = now_us;

    while (1) {
        now_us = esp_timer_get_time();

        (void)button_update(&b_center, PIN_SW_CENTER, now_us);
        bool left_edge = button_update(&b_left, PIN_SW_LEFT, now_us);
        bool right_edge = button_update(&b_right, PIN_SW_RIGHT, now_us);

        /* LEFT/RIGHT: octave step on stable press (falling edge). */
        if (left_edge && is_pressed_level(b_left.stable_level)) {
            s_octave = (int8_t)clamp_i8((int)s_octave - 1, -3, 3);
            ESP_LOGI(TAG, "octave=%d", (int)s_octave);
        }
        if (right_edge && is_pressed_level(b_right.stable_level)) {
            s_octave = (int8_t)clamp_i8((int)s_octave + 1, -3, 3);
            ESP_LOGI(TAG, "octave=%d", (int)s_octave);
        }

        /* CENTER: short press toggles MPE, long press triggers BLE pairing hook.
         * Long press suppresses the short action on release.
         */
        bool center_pressed = is_pressed_level(b_center.stable_level);
        if (center_pressed && !b_center.pressed) {
            b_center.pressed = true;
            b_center.press_start_us = now_us;
            b_center.long_fired = false;
        } else if (!center_pressed && b_center.pressed) {
            /* release */
            b_center.pressed = false;
            if (!b_center.long_fired) {
                bool new_en = !midi_mpe_is_enabled();
                midi_mpe_set_enabled(new_en);
                ESP_LOGI(TAG, "SW_CENTER short: MPE %s", new_en ? "ENABLED" : "DISABLED");
            }
        } else if (center_pressed && b_center.pressed && !b_center.long_fired) {
            if ((now_us - b_center.press_start_us) >= CONTROLS_CENTER_LONGPRESS_US) {
                b_center.long_fired = true;
                ESP_LOGI(TAG, "SW_CENTER long: BLE pairing request");
                emiuet_ble_pairing_request();
            }
        }

        vTaskDelay(delay_ticks);
    }
}

void controls_start(void)
{
    if (s_task) return;

    /* Pins are configured in board_pins_init_early(). */
    xTaskCreate(controls_task, "controls", 4096, NULL, 6, &s_task);
}

int8_t controls_get_octave(void)
{
    return s_octave;
}
