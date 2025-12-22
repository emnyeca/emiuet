#include "slider.h"
#include "board_pins.h"
#include "adc_manager.h"
#include "esp_err.h"
#include "esp_log.h"
#include <limits.h>

static const char *TAG = "slider";

/* EMA filter state */
/* EMA filter state and initialization guard */
static float pb_ema = 0.0f;
static bool pb_ema_initialized = false;
static float mod_ema = 0.0f;
static float vel_ema = 0.0f;
static bool s_enabled = false;
/* Last-good raw (0..1023) to return on transient ADC failures */
static int s_last_raw = 0;
static int s_adc_fail_count = 0;
#ifdef SLIDER_DEBUG_CALIB
/* Observed min/max for optional calibration logging */
static int s_observed_min = INT_MAX;
static int s_observed_max = INT_MIN;
#endif

void slider_init(void)
{
    /* adc_manager_init() is idempotent */
    s_enabled = adc_manager_init();
    if (!s_enabled) {
        ESP_LOGW(TAG, "ADC manager not available; sliders disabled");
    }
}

static uint16_t read_adc_gpio_or_last(gpio_num_t gpio)
{
    if (!s_enabled) return (uint16_t)s_last_raw;

    int raw = 0;
    esp_err_t ret = adc_manager_read_raw(gpio, &raw);
    if (ret != ESP_OK) {
        s_adc_fail_count++;
        if (s_adc_fail_count == 1) {
            ESP_LOGW(TAG, "adc read failed for gpio %d (first failure)", (int)gpio);
        } else if (s_adc_fail_count == 8) {
            ESP_LOGW(TAG, "adc read failing repeatedly (%d times) - returning last good value", s_adc_fail_count);
        }
        return (uint16_t)s_last_raw;
    }

    s_adc_fail_count = 0;

    /* Map raw (0..4095) to 0..1023 */
    if (raw < 0) raw = 0;
    if (raw > 4095) raw = 4095;
    int mapped = (int)((raw * 1023) / 4095);
    s_last_raw = mapped;

#ifdef SLIDER_DEBUG_CALIB
    if (mapped < s_observed_min) {
        s_observed_min = mapped;
        ESP_LOGD(TAG, "slider observed min=%d", s_observed_min);
    }
    if (mapped > s_observed_max) {
        s_observed_max = mapped;
        ESP_LOGD(TAG, "slider observed max=%d", s_observed_max);
    }
#endif

    return (uint16_t)mapped;
}

uint16_t slider_read_pitchbend(void)
{
    if (!s_enabled) return 0;

    /* Multi-sample with simple trimming to reject spikes */
#ifndef SLIDER_ADC_SAMPLES
#define SLIDER_ADC_SAMPLES 8
#endif
    const int samples = SLIDER_ADC_SAMPLES;
    int minv = INT_MAX;
    int maxv = INT_MIN;
    int sum = 0;
    for (int i = 0; i < samples; ++i) {
        int v = read_adc_gpio_or_last(PIN_SLIDER_PB);
        sum += v;
        if (v < minv) minv = v;
        if (v > maxv) maxv = v;
    }

    /* Trim one min and one max if samples >=3 (robust against spikes) */
    int denom = samples;
    if (samples >= 3) {
        sum -= minv + maxv;
        denom -= 2;
    }
    float avg = (denom > 0) ? ((float)sum / (float)denom) : 0.0f;

    /* Clamp into expected range */
    if (avg < 0.0f) avg = 0.0f;
    if (avg > 1023.0f) avg = 1023.0f;

    /* initialize EMA on first stable observation to avoid startup wander */
#ifndef SLIDER_ADC_EMA_ALPHA
#define SLIDER_ADC_EMA_ALPHA 0.12f
#endif
    const float alpha = SLIDER_ADC_EMA_ALPHA;
    if (!pb_ema_initialized) {
        pb_ema = avg;
        pb_ema_initialized = true;
        ESP_LOGD(TAG, "pb_ema initialized=%.2f", pb_ema);
    } else {
        pb_ema = pb_ema * (1.0f - alpha) + avg * alpha;
    }

    /* Return 0..1023 (smoothed) */
    return (uint16_t)pb_ema;
}

uint16_t slider_read_mod(void)
{
    if (!s_enabled) return 0;
    uint16_t v = read_adc_gpio_or_last(PIN_SLIDER_MOD);
    float nv = (float)v;
    mod_ema = mod_ema * 0.9f + nv * 0.1f;
    return (uint16_t)mod_ema;
}

uint16_t slider_read_velocity(void)
{
    if (!s_enabled) return 0;
    uint16_t v = read_adc_gpio_or_last(PIN_SLIDER_VEL);
    float nv = (float)v;
    vel_ema = vel_ema * 0.9f + nv * 0.1f;
    return (uint16_t)vel_ema;
}

bool slider_is_enabled(void)
{
    return s_enabled;
}
