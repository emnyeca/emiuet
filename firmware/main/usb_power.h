#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    USB_CURRENT_DEFAULT = 0,
    USB_CURRENT_1P5A,
    USB_CURRENT_3A,
    USB_CURRENT_UNKNOWN,
} usb_current_mode_t;

typedef struct {
    bool attached_as_sink;
    bool orientation_cc2;
    usb_current_mode_t advertised_current;
    uint16_t rgb_budget_ma;
} usb_power_status_t;

void usb_power_set_status(bool attached_as_sink, bool orientation_cc2,
                          usb_current_mode_t current);
usb_power_status_t usb_power_get_status(void);
