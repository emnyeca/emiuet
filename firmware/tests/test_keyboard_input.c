#include <stdbool.h>
#include <stdint.h>

#include "keyboard_input.h"
#include "keyboard_keymap.h"
#include "usb_hid_keyboard.h"

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

void __main(void) {}

void *memset(void *destination, int value, unsigned long length)
{
    unsigned char *bytes = (unsigned char *)destination;
    for (unsigned long i = 0; i < length; ++i) bytes[i] = (unsigned char)value;
    return destination;
}

static uint8_t s_modifiers;
static uint8_t s_keycodes[USB_HID_KEYBOARD_MAX_KEYS];

bool usb_hid_keyboard_send_report(uint8_t modifiers,
                                  const uint8_t keycodes[USB_HID_KEYBOARD_MAX_KEYS])
{
    s_modifiers = modifiers;
    for (int i = 0; i < USB_HID_KEYBOARD_MAX_KEYS; ++i) s_keycodes[i] = keycodes[i];
    return true;
}

void usb_hid_keyboard_release_all(void)
{
    const uint8_t empty[USB_HID_KEYBOARD_MAX_KEYS] = {0};
    (void)usb_hid_keyboard_send_report(0, empty);
}

static bool report_has(uint8_t usage)
{
    for (int i = 0; i < USB_HID_KEYBOARD_MAX_KEYS; ++i) {
        if (s_keycodes[i] == usage) return true;
    }
    return false;
}

int main(void)
{
    /* Shift+A */
    keyboard_input_handle_event(3, 0, true);
    keyboard_input_handle_event(2, 1, true);
    CHECK((s_modifiers & EM_HID_MOD_LEFT_SHIFT) != 0);
    CHECK(report_has(EM_HID_A));
    keyboard_input_release_all();

    /* Ctrl+C, then replace C with V and Z while Ctrl remains held. */
    keyboard_input_handle_event(4, 0, true);
    keyboard_input_handle_event(3, 3, true);
    CHECK((s_modifiers & EM_HID_MOD_LEFT_CTRL) != 0);
    CHECK(report_has(EM_HID_C));
    keyboard_input_handle_event(3, 3, false);
    keyboard_input_handle_event(3, 4, true);
    CHECK(report_has(EM_HID_V));
    keyboard_input_handle_event(3, 4, false);
    keyboard_input_handle_event(3, 1, true);
    CHECK(report_has(EM_HID_Z));
    keyboard_input_release_all();

    /* Fn changes the selected layer for every held physical key. */
    keyboard_input_handle_event(0, 1, true);
    CHECK(report_has(EM_HID_1));
    keyboard_input_handle_event(4, 3, true);
    CHECK(keyboard_input_get_layer() == KEYBOARD_LAYER_FUNCTION);
    CHECK(report_has(EM_HID_F1));
    CHECK(!report_has(EM_HID_1));
    keyboard_input_release_all();

    CHECK(s_modifiers == 0);
    for (int i = 0; i < USB_HID_KEYBOARD_MAX_KEYS; ++i) CHECK(s_keycodes[i] == 0);
    return 0;
}
