#include "keyboard_keymap.h"

#define N       { KEYMAP_ACTION_NONE, 0 }
#define K(code) { KEYMAP_ACTION_KEY, (code) }
#define M(mod)  { KEYMAP_ACTION_MODIFIER, (mod) }
#define FN      { KEYMAP_ACTION_FN, 0 }
#define MODE    { KEYMAP_ACTION_MODE, 0 }

/*
 * Single source of truth for physical matrix position -> HID action.
 * Four corners are reserved for the deliberate 2-second mode chord.
 */
static const keymap_action_t s_keymap[KEYBOARD_LAYER_COUNT][KEYBOARD_MATRIX_ROWS][KEYBOARD_MATRIX_COLS] = {
    [KEYBOARD_LAYER_BASE] = {
        { MODE, K(EM_HID_1), K(EM_HID_2), K(EM_HID_3), K(EM_HID_4), K(EM_HID_5), K(EM_HID_6), K(EM_HID_7), K(EM_HID_8), K(EM_HID_9), K(EM_HID_0), K(EM_HID_MINUS), MODE },
        { K(EM_HID_TAB), K(EM_HID_Q), K(EM_HID_W), K(EM_HID_E), K(EM_HID_R), K(EM_HID_T), K(EM_HID_Y), K(EM_HID_U), K(EM_HID_I), K(EM_HID_O), K(EM_HID_P), K(EM_HID_BRACKET_LEFT), K(EM_HID_BRACKET_RIGHT) },
        { K(EM_HID_CAPS_LOCK), K(EM_HID_A), K(EM_HID_S), K(EM_HID_D), K(EM_HID_F), K(EM_HID_G), K(EM_HID_H), K(EM_HID_J), K(EM_HID_K), K(EM_HID_L), K(EM_HID_SEMICOLON), K(EM_HID_APOSTROPHE), K(EM_HID_ENTER) },
        { M(EM_HID_MOD_LEFT_SHIFT), K(EM_HID_Z), K(EM_HID_X), K(EM_HID_C), K(EM_HID_V), K(EM_HID_B), K(EM_HID_N), K(EM_HID_M), K(EM_HID_COMMA), K(EM_HID_PERIOD), K(EM_HID_SLASH), M(EM_HID_MOD_RIGHT_SHIFT), K(EM_HID_BACKSLASH) },
        { M(EM_HID_MOD_LEFT_CTRL), M(EM_HID_MOD_LEFT_GUI), M(EM_HID_MOD_LEFT_ALT), FN, K(EM_HID_SPACE), K(EM_HID_SPACE), K(EM_HID_SPACE), K(EM_HID_SPACE), K(EM_HID_SPACE), M(EM_HID_MOD_RIGHT_ALT), K(EM_HID_ARROW_LEFT), K(EM_HID_ARROW_DOWN), K(EM_HID_ARROW_RIGHT) },
        { MODE, K(EM_HID_ESCAPE), K(EM_HID_GRAVE), K(EM_HID_EQUAL), K(EM_HID_BACKSPACE), K(EM_HID_DELETE), K(EM_HID_INSERT), K(EM_HID_APPLICATION), K(EM_HID_HOME), K(EM_HID_END), K(EM_HID_PAGE_UP), K(EM_HID_ARROW_UP), MODE },
    },
    [KEYBOARD_LAYER_FUNCTION] = {
        { MODE, K(EM_HID_F1), K(EM_HID_F2), K(EM_HID_F3), K(EM_HID_F4), K(EM_HID_F5), K(EM_HID_F6), K(EM_HID_F7), K(EM_HID_F8), K(EM_HID_F9), K(EM_HID_F10), K(EM_HID_F11), MODE },
        { K(EM_HID_TAB), K(EM_HID_HOME), K(EM_HID_ARROW_UP), K(EM_HID_END), N, N, N, N, N, N, K(EM_HID_PRINT_SCREEN), K(EM_HID_SCROLL_LOCK), K(EM_HID_PAUSE) },
        { K(EM_HID_CAPS_LOCK), K(EM_HID_ARROW_LEFT), K(EM_HID_ARROW_DOWN), K(EM_HID_ARROW_RIGHT), N, N, N, N, N, N, N, N, K(EM_HID_ENTER) },
        { M(EM_HID_MOD_LEFT_SHIFT), N, N, N, N, N, N, N, N, N, N, M(EM_HID_MOD_RIGHT_SHIFT), N },
        { M(EM_HID_MOD_LEFT_CTRL), M(EM_HID_MOD_LEFT_GUI), M(EM_HID_MOD_LEFT_ALT), FN, K(EM_HID_SPACE), K(EM_HID_SPACE), K(EM_HID_SPACE), K(EM_HID_SPACE), K(EM_HID_SPACE), M(EM_HID_MOD_RIGHT_ALT), K(EM_HID_HOME), K(EM_HID_PAGE_DOWN), K(EM_HID_END) },
        { MODE, K(EM_HID_F12), K(EM_HID_ESCAPE), K(EM_HID_EQUAL), K(EM_HID_BACKSPACE), K(EM_HID_DELETE), K(EM_HID_INSERT), K(EM_HID_APPLICATION), K(EM_HID_HOME), K(EM_HID_END), K(EM_HID_PAGE_UP), K(EM_HID_ARROW_UP), MODE },
    },
};

const keymap_action_t *keyboard_keymap_get(uint8_t layer, int row, int col)
{
    if (layer >= KEYBOARD_LAYER_COUNT || row < 0 || row >= KEYBOARD_MATRIX_ROWS ||
        col < 0 || col >= KEYBOARD_MATRIX_COLS) {
        return 0;
    }
    return &s_keymap[layer][row][col];
}

bool keyboard_keymap_is_fn(int row, int col)
{
    const keymap_action_t *action = keyboard_keymap_get(KEYBOARD_LAYER_BASE, row, col);
    return action && action->type == KEYMAP_ACTION_FN;
}

bool keyboard_keymap_is_mode_chord_key(int row, int col)
{
    const keymap_action_t *action = keyboard_keymap_get(KEYBOARD_LAYER_BASE, row, col);
    return action && action->type == KEYMAP_ACTION_MODE;
}
