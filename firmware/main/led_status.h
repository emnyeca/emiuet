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
    LED_ST_FAULT,          // ★追加（推奨）
    LED_ST_COUNT
} led_state_t;

void led_status_start(void);
void led_status_set_state(led_state_t st);

#ifdef __cplusplus
}
#endif
