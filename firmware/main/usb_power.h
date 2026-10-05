#pragma once

#include <stdbool.h>
#include <stdint.h>

/* The CC detector reports only "below" or "at/above" the Type-C 1.5 A threshold.
 * 3 A advertisements are intentionally treated like 1.5 A (Rev.B ceiling).
 */
typedef enum {
    USB_CURRENT_UNKNOWN = 0,
    USB_CURRENT_DEFAULT,
    USB_CURRENT_1P5A_OR_MORE,
} usb_current_mode_t;

typedef struct {
    usb_current_mode_t advertised_current;
    uint16_t rgb_budget_ma;
} usb_power_status_t;

void usb_power_set_advertised_current(usb_current_mode_t current);
/* Default-current RGB is allowed only while USB is configured and active.
 * This limits RGB output, not total board current or LED idle consumption.
 */
void usb_power_set_bus_state(bool configured, bool suspended);
usb_power_status_t usb_power_get_status(void);
