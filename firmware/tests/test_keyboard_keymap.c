#include <stdbool.h>
#include <stdint.h>

#include "keyboard_keymap.h"

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

/* Host test links without a C runtime; clang's MinGW target still calls this hook. */
void __main(void) {}

static bool layer_has_key(uint8_t layer, uint8_t usage)
{
    for (int row = 0; row < KEYBOARD_MATRIX_ROWS; ++row) {
        for (int col = 0; col < KEYBOARD_MATRIX_COLS; ++col) {
            const keymap_action_t *action = keyboard_keymap_get(layer, row, col);
            if (action && action->type == KEYMAP_ACTION_KEY && action->value == usage) return true;
        }
    }
    return false;
}

static bool base_has_modifier(uint8_t modifier)
{
    for (int row = 0; row < KEYBOARD_MATRIX_ROWS; ++row) {
        for (int col = 0; col < KEYBOARD_MATRIX_COLS; ++col) {
            const keymap_action_t *action = keyboard_keymap_get(KEYBOARD_LAYER_BASE, row, col);
            if (action && action->type == KEYMAP_ACTION_MODIFIER && action->value == modifier) return true;
        }
    }
    return false;
}

int main(void)
{
    for (uint8_t usage = EM_HID_A; usage <= EM_HID_Z; ++usage) {
        CHECK(layer_has_key(KEYBOARD_LAYER_BASE, usage));
    }
    for (uint8_t usage = EM_HID_1; usage <= EM_HID_0; ++usage) {
        CHECK(layer_has_key(KEYBOARD_LAYER_BASE, usage));
    }

    const uint8_t required_base_keys[] = {
        EM_HID_ESCAPE, EM_HID_TAB, EM_HID_SPACE, EM_HID_ENTER,
        EM_HID_BACKSPACE, EM_HID_DELETE, EM_HID_ARROW_LEFT,
        EM_HID_ARROW_DOWN, EM_HID_ARROW_RIGHT, EM_HID_ARROW_UP,
        EM_HID_MINUS, EM_HID_EQUAL, EM_HID_BRACKET_LEFT,
        EM_HID_BRACKET_RIGHT, EM_HID_BACKSLASH, EM_HID_SEMICOLON,
        EM_HID_APOSTROPHE, EM_HID_GRAVE, EM_HID_COMMA,
        EM_HID_PERIOD, EM_HID_SLASH,
    };
    for (unsigned int i = 0; i < sizeof(required_base_keys); ++i) {
        CHECK(layer_has_key(KEYBOARD_LAYER_BASE, required_base_keys[i]));
    }

    CHECK(base_has_modifier(EM_HID_MOD_LEFT_CTRL));
    CHECK(base_has_modifier(EM_HID_MOD_LEFT_SHIFT));
    CHECK(base_has_modifier(EM_HID_MOD_LEFT_ALT));
    CHECK(base_has_modifier(EM_HID_MOD_LEFT_GUI));

    for (uint8_t usage = EM_HID_F1; usage <= EM_HID_F12; ++usage) {
        CHECK(layer_has_key(KEYBOARD_LAYER_FUNCTION, usage));
    }

    CHECK(keyboard_keymap_is_fn(4, 3));
    CHECK(keyboard_keymap_is_mode_chord_key(0, 0));
    CHECK(keyboard_keymap_is_mode_chord_key(0, 12));
    CHECK(keyboard_keymap_is_mode_chord_key(5, 0));
    CHECK(keyboard_keymap_is_mode_chord_key(5, 12));
    CHECK(!keyboard_keymap_is_mode_chord_key(2, 6));

    return 0;
}
