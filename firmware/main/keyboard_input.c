#include "keyboard_input.h"

#include "keyboard_keymap.h"
#include "usb_hid_keyboard.h"

static bool s_pressed[KEYBOARD_MATRIX_ROWS][KEYBOARD_MATRIX_COLS];
static uint8_t s_layer = KEYBOARD_LAYER_BASE;

static void rebuild_and_send_report(void)
{
    uint8_t modifiers = 0;
    uint8_t keycodes[USB_HID_KEYBOARD_MAX_KEYS] = {0};
    uint8_t key_count = 0;
    bool fn_pressed = false;

    for (int row = 0; row < KEYBOARD_MATRIX_ROWS; ++row) {
        for (int col = 0; col < KEYBOARD_MATRIX_COLS; ++col) {
            if (s_pressed[row][col] && keyboard_keymap_is_fn(row, col)) {
                fn_pressed = true;
            }
        }
    }

    s_layer = fn_pressed ? KEYBOARD_LAYER_FUNCTION : KEYBOARD_LAYER_BASE;

    for (int row = 0; row < KEYBOARD_MATRIX_ROWS; ++row) {
        for (int col = 0; col < KEYBOARD_MATRIX_COLS; ++col) {
            if (!s_pressed[row][col]) continue;

            const keymap_action_t *action = keyboard_keymap_get(s_layer, row, col);
            if (!action) continue;

            if (action->type == KEYMAP_ACTION_MODIFIER) {
                modifiers |= action->value;
            } else if (action->type == KEYMAP_ACTION_KEY && key_count < USB_HID_KEYBOARD_MAX_KEYS) {
                bool duplicate = false;
                for (uint8_t i = 0; i < key_count; ++i) {
                    if (keycodes[i] == action->value) duplicate = true;
                }
                if (!duplicate) keycodes[key_count++] = action->value;
            }
        }
    }

    (void)usb_hid_keyboard_send_report(modifiers, keycodes);
}

void keyboard_input_handle_event(int row, int col, bool pressed)
{
    if (row < 0 || row >= KEYBOARD_MATRIX_ROWS || col < 0 || col >= KEYBOARD_MATRIX_COLS) return;
    s_pressed[row][col] = pressed;
    rebuild_and_send_report();
}

void keyboard_input_release_all(void)
{
    for (int row = 0; row < KEYBOARD_MATRIX_ROWS; ++row) {
        for (int col = 0; col < KEYBOARD_MATRIX_COLS; ++col) {
            s_pressed[row][col] = false;
        }
    }
    s_layer = KEYBOARD_LAYER_BASE;
    usb_hid_keyboard_release_all();
}

uint8_t keyboard_input_get_layer(void)
{
    return s_layer;
}
