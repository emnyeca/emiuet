#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// LED "state" (what we want to express)
typedef enum {
    LED_ST_OFF = 0,
    LED_ST_SYSTEM_NORMAL,
    LED_ST_BLE_ADV,
    LED_ST_CHARGING,
    LED_ST_CHARGED,
    LED_ST_LOW_BATT,
    LED_ST_SLEEP,

    LED_ST_COUNT
} led_state_t;

// Start LED status task (includes auto demo: cycles states periodically)
void led_status_start(void);

#ifdef __cplusplus
}
#endif
