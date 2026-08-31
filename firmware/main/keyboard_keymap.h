#pragma once

#include <stdbool.h>
#include <stdint.h>

#define KEYBOARD_MATRIX_ROWS 6
#define KEYBOARD_MATRIX_COLS 13

typedef enum {
    KEYMAP_ACTION_NONE = 0,
    KEYMAP_ACTION_KEY,
    KEYMAP_ACTION_MODIFIER,
    KEYMAP_ACTION_FN,
    KEYMAP_ACTION_MODE,
} keymap_action_type_t;

typedef struct {
    keymap_action_type_t type;
    uint8_t value;
} keymap_action_t;

enum {
    KEYBOARD_LAYER_BASE = 0,
    KEYBOARD_LAYER_FUNCTION,
    KEYBOARD_LAYER_COUNT,
};

/* USB HID Keyboard/Keypad usages used by Emiuet's keymap. */
enum {
    EM_HID_A = 0x04,
    EM_HID_B = 0x05,
    EM_HID_C = 0x06,
    EM_HID_D = 0x07,
    EM_HID_E = 0x08,
    EM_HID_F = 0x09,
    EM_HID_G = 0x0A,
    EM_HID_H = 0x0B,
    EM_HID_I = 0x0C,
    EM_HID_J = 0x0D,
    EM_HID_K = 0x0E,
    EM_HID_L = 0x0F,
    EM_HID_M = 0x10,
    EM_HID_N = 0x11,
    EM_HID_O = 0x12,
    EM_HID_P = 0x13,
    EM_HID_Q = 0x14,
    EM_HID_R = 0x15,
    EM_HID_S = 0x16,
    EM_HID_T = 0x17,
    EM_HID_U = 0x18,
    EM_HID_V = 0x19,
    EM_HID_W = 0x1A,
    EM_HID_X = 0x1B,
    EM_HID_Y = 0x1C,
    EM_HID_Z = 0x1D,
    EM_HID_1 = 0x1E,
    EM_HID_2 = 0x1F,
    EM_HID_3 = 0x20,
    EM_HID_4 = 0x21,
    EM_HID_5 = 0x22,
    EM_HID_6 = 0x23,
    EM_HID_7 = 0x24,
    EM_HID_8 = 0x25,
    EM_HID_9 = 0x26,
    EM_HID_0 = 0x27,
    EM_HID_ENTER = 0x28,
    EM_HID_ESCAPE = 0x29,
    EM_HID_BACKSPACE = 0x2A,
    EM_HID_TAB = 0x2B,
    EM_HID_SPACE = 0x2C,
    EM_HID_MINUS = 0x2D,
    EM_HID_EQUAL = 0x2E,
    EM_HID_BRACKET_LEFT = 0x2F,
    EM_HID_BRACKET_RIGHT = 0x30,
    EM_HID_BACKSLASH = 0x31,
    EM_HID_SEMICOLON = 0x33,
    EM_HID_APOSTROPHE = 0x34,
    EM_HID_GRAVE = 0x35,
    EM_HID_COMMA = 0x36,
    EM_HID_PERIOD = 0x37,
    EM_HID_SLASH = 0x38,
    EM_HID_CAPS_LOCK = 0x39,
    EM_HID_F1 = 0x3A,
    EM_HID_F2 = 0x3B,
    EM_HID_F3 = 0x3C,
    EM_HID_F4 = 0x3D,
    EM_HID_F5 = 0x3E,
    EM_HID_F6 = 0x3F,
    EM_HID_F7 = 0x40,
    EM_HID_F8 = 0x41,
    EM_HID_F9 = 0x42,
    EM_HID_F10 = 0x43,
    EM_HID_F11 = 0x44,
    EM_HID_F12 = 0x45,
    EM_HID_PRINT_SCREEN = 0x46,
    EM_HID_SCROLL_LOCK = 0x47,
    EM_HID_PAUSE = 0x48,
    EM_HID_INSERT = 0x49,
    EM_HID_HOME = 0x4A,
    EM_HID_PAGE_UP = 0x4B,
    EM_HID_DELETE = 0x4C,
    EM_HID_END = 0x4D,
    EM_HID_PAGE_DOWN = 0x4E,
    EM_HID_ARROW_RIGHT = 0x4F,
    EM_HID_ARROW_LEFT = 0x50,
    EM_HID_ARROW_DOWN = 0x51,
    EM_HID_ARROW_UP = 0x52,
    EM_HID_APPLICATION = 0x65,
};

enum {
    EM_HID_MOD_LEFT_CTRL = 1u << 0,
    EM_HID_MOD_LEFT_SHIFT = 1u << 1,
    EM_HID_MOD_LEFT_ALT = 1u << 2,
    EM_HID_MOD_LEFT_GUI = 1u << 3,
    EM_HID_MOD_RIGHT_CTRL = 1u << 4,
    EM_HID_MOD_RIGHT_SHIFT = 1u << 5,
    EM_HID_MOD_RIGHT_ALT = 1u << 6,
    EM_HID_MOD_RIGHT_GUI = 1u << 7,
};

const keymap_action_t *keyboard_keymap_get(uint8_t layer, int row, int col);
bool keyboard_keymap_is_fn(int row, int col);
bool keyboard_keymap_is_mode_chord_key(int row, int col);
