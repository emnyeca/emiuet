#pragma once

#include <stdbool.h>
#include <stdint.h>

#define USB_HID_KEYBOARD_MAX_KEYS 6

bool usb_hid_keyboard_init(void);
bool usb_hid_keyboard_send_report(uint8_t modifiers,
                                  const uint8_t keycodes[USB_HID_KEYBOARD_MAX_KEYS]);
void usb_hid_keyboard_release_all(void);
void usb_hid_keyboard_disconnected(void);
void usb_hid_keyboard_set_led_report(uint8_t leds);
bool usb_hid_keyboard_caps_lock_on(void);
