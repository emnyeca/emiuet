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
    .advertised_current = USB_CURRENT_UNKNOWN,
    .rgb_budget_ma = 0,
};
static bool s_usb_configured;
static bool s_usb_suspended;

static void update_budget(void)
{
    s_status.rgb_budget_ma = 0;
    if (s_status.advertised_current == USB_CURRENT_1P5A_OR_MORE) {
        s_status.rgb_budget_ma = CONFIG_EMIUET_RGB_1P5A_BUDGET_MA;
    } else if (s_status.advertised_current == USB_CURRENT_DEFAULT &&
               s_usb_configured && !s_usb_suspended) {
        s_status.rgb_budget_ma = CONFIG_EMIUET_RGB_DEFAULT_BUDGET_MA;
    }
}

void usb_power_set_advertised_current(usb_current_mode_t current)
{
    portENTER_CRITICAL(&s_mux);
    s_status.advertised_current = current;
    update_budget();
    portEXIT_CRITICAL(&s_mux);
}

void usb_power_set_bus_state(bool configured, bool suspended)
{
    portENTER_CRITICAL(&s_mux);
    s_usb_configured = configured;
    s_usb_suspended = suspended;
    update_budget();
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
