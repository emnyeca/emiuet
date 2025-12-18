#include "led_status.h"

#include <stdio.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/ledc.h"

// ============================================================
// LED HW (GPIO6 + LEDC PWM)
// ------------------------------------------------------------
// Wiring assumed (Active-Low):
//   3V3 -> R(1k) -> LED Anode -> LED Cathode -> GPIO6
// So:
//   GPIO LOW  = ON
//   GPIO HIGH = OFF
// We implement brightness 0..255 and invert duty for Active-Low.
// ============================================================

#define LED_GPIO          (6)

// LEDC config
#define LEDC_MODE         LEDC_LOW_SPEED_MODE
#define LEDC_TIMER        LEDC_TIMER_0
#define LEDC_CHANNEL      LEDC_CHANNEL_0
#define LEDC_DUTY_RES     LEDC_TIMER_13_BIT           // 0..8191
#define LEDC_FREQUENCY_HZ (4000)                      // flicker-free enough

// Active-Low
#define LED_ACTIVE_LOW    (1)

static const int MAX_DUTY = (1 << 13) - 1;

static void led_hw_init(void)
{
    ledc_timer_config_t tconf = {
        .speed_mode = LEDC_MODE,
        .duty_resolution = LEDC_DUTY_RES,
        .timer_num = LEDC_TIMER,
        .freq_hz = LEDC_FREQUENCY_HZ,
        .clk_cfg = LEDC_AUTO_CLK
    };
    (void)ledc_timer_config(&tconf);

    ledc_channel_config_t cconf = {
        .gpio_num = LED_GPIO,
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL,
        .timer_sel = LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
        .intr_type = LEDC_INTR_DISABLE,
        .flags.output_invert = 0,   // we handle inversion in duty
    };
    (void)ledc_channel_config(&cconf);

    // start OFF
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LED_ACTIVE_LOW ? MAX_DUTY : 0);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);

    printf("[LedHW] LEDC init: GPIO%d, %dHz, %dbit, active-%s\n",
           LED_GPIO, LEDC_FREQUENCY_HZ, 13, LED_ACTIVE_LOW ? "LOW" : "HIGH");
}

static void led_hw_apply(uint8_t level_0_255)
{
    // Map 0..255 -> 0..MAX_DUTY (linear)
    int duty = (int)((level_0_255 * (uint32_t)MAX_DUTY) / 255u);

#if LED_ACTIVE_LOW
    // Active-Low: ON means GPIO low => duty must be inverted
    duty = MAX_DUTY - duty;
#endif

    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

// ============================================================
// Pattern engine
// ============================================================

static const char* led_state_name(led_state_t st)
{
    switch (st) {
        case LED_ST_OFF:           return "OFF";
        case LED_ST_SYSTEM_NORMAL: return "SYSTEM_NORMAL";
        case LED_ST_BLE_ADV:       return "BLE_ADV";
        case LED_ST_CHARGING:      return "CHARGING";
        case LED_ST_CHARGED:       return "CHARGED";
        case LED_ST_LOW_BATT:      return "LOW_BATT";
        case LED_ST_SLEEP:         return "SLEEP";
        default:                   return "UNKNOWN";
    }
}

static uint8_t pattern_brightness_for_tick(led_state_t st, uint32_t t_ms_in_state)
{
    // Tune later (0..255)
    const uint8_t DIM  = 25;
    const uint8_t MID  = 80;
    const uint8_t HIGH = 200;

    switch (st) {
        case LED_ST_OFF:
            return 0;

        case LED_ST_SLEEP:
            // For now, fully off (you can change to ultra-dim later)
            return 0;

        case LED_ST_SYSTEM_NORMAL:
            // low brightness steady
            return DIM;

        case LED_ST_CHARGED:
            // high brightness steady
            return HIGH;

        case LED_ST_CHARGING: {
            // 1 Hz blink: 500ms ON / 500ms OFF
            uint32_t phase = t_ms_in_state % 1000;
            return (phase < 500) ? MID : 0;
        }

        case LED_ST_LOW_BATT: {
            // 4 Hz blink: 125ms ON / 125ms OFF (period 250ms)
            uint32_t phase = t_ms_in_state % 250;
            return (phase < 125) ? HIGH : 0;
        }

        case LED_ST_BLE_ADV: {
            // 10 Hz pulse feel: 100ms period, ON 20ms
            // (with faint base DIM for "alive" feel)
            uint32_t phase = t_ms_in_state % 100;
            return (phase < 20) ? HIGH : DIM;
        }

        default:
            return 0;
    }
}

// ============================================================
// Auto demo task (single task)
// ============================================================

static void led_status_task(void *arg)
{
    (void)arg;

    const uint32_t TICK_MS = 20;
    const TickType_t tick = pdMS_TO_TICKS(TICK_MS);
    TickType_t last_wake = xTaskGetTickCount();

    const uint32_t DEMO_STEP_MS = 5000;

    led_state_t state = LED_ST_SYSTEM_NORMAL;
    uint32_t t_in_state = 0;

    printf("[LedStatus] task started (auto demo). tick=%ums, demo_step=%ums\n",
           (unsigned)TICK_MS, (unsigned)DEMO_STEP_MS);
    printf("[LedStatus] initial state: %s\n", led_state_name(state));

    while (1) {
        uint8_t level = pattern_brightness_for_tick(state, t_in_state);
        led_hw_apply(level);

        t_in_state += TICK_MS;

        if (t_in_state >= DEMO_STEP_MS) {
            t_in_state = 0;
            state = (led_state_t)((state + 1) % LED_ST_COUNT);
            printf("[LedStatus] state -> %s\n", led_state_name(state));
        }

        vTaskDelayUntil(&last_wake, tick);
    }
}

void led_status_start(void)
{
    led_hw_init();

    // Single task only, as requested
    xTaskCreatePinnedToCore(led_status_task, "LedStatus", 4096, NULL, 3, NULL, 0);
}
