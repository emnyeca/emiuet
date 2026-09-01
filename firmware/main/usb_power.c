#include "usb_power.h"

#include "freertos/FreeRTOS.h"
#include "sdkconfig.h"

#ifndef CONFIG_EMIUET_RGB_DEFAULT_BUDGET_MA
#define CONFIG_EMIUET_RGB_DEFAULT_BUDGET_MA 200
#endif
#ifndef CONFIG_EMIUET_RGB_1P5A_BUDGET_MA
#define CONFIG_EMIUET_RGB_1P5A_BUDGET_MA 1000
#endif

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static usb_power_status_t s_status = {
    .attached_as_sink = false,
    .orientation_cc2 = false,
    .advertised_current = USB_CURRENT_UNKNOWN,
    .rgb_budget_ma = CONFIG_EMIUET_RGB_DEFAULT_BUDGET_MA,
};

void usb_power_set_status(bool attached_as_sink, bool orientation_cc2,
                          usb_current_mode_t current)
{
    const uint16_t budget = (current == USB_CURRENT_1P5A || current == USB_CURRENT_3A)
                                ? CONFIG_EMIUET_RGB_1P5A_BUDGET_MA
                                : CONFIG_EMIUET_RGB_DEFAULT_BUDGET_MA;
    portENTER_CRITICAL(&s_mux);
    s_status.attached_as_sink = attached_as_sink;
    s_status.orientation_cc2 = orientation_cc2;
    s_status.advertised_current = current;
    s_status.rgb_budget_ma = budget;
    portEXIT_CRITICAL(&s_mux);
}

usb_power_status_t usb_power_get_status(void)
{
    usb_power_status_t copy;
    portENTER_CRITICAL(&s_mux);
    copy = s_status;
    portEXIT_CRITICAL(&s_mux);
    return copy;
}
