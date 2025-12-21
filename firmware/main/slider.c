#include "slider.h"
#include "board_pins.h"
#include "esp_err.h"
#include "esp_log.h"
#include <limits.h>

#if __has_include("esp_adc/adc_oneshot.h")
#include "esp_adc/adc_oneshot.h"
#define HAVE_ADC_ONESHOT 1
#else
#include "driver/adc.h"
#define HAVE_ADC_ONESHOT 0
#endif

static const char *TAG = "slider";

/* EMA filter state */
/* EMA filter state and initialization guard */
static float pb_ema = 0.0f;
static bool pb_ema_initialized = false;
static float mod_ema = 0.0f;
static float vel_ema = 0.0f;

#if HAVE_ADC_ONESHOT
static adc_oneshot_unit_handle_t adc_unit_handle = NULL;
static bool adc_initialized = false;
static bool s_enabled = false;
/* Last-good raw (0..1023) to return on transient ADC failures */
static int s_last_raw = 0;
static int s_adc_fail_count = 0;
/* Observed min/max for optional calibration logging */
static int s_observed_min = INT_MAX;
static int s_observed_max = INT_MIN;

void slider_init(void)
{
    if (adc_initialized) return;

#if defined(SLIDER_PB_ADC_CHANNEL) || defined(SLIDER_MOD_ADC_CHANNEL) || defined(SLIDER_VEL_ADC_CHANNEL)
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    esp_err_t ret = adc_oneshot_new_unit(&init_cfg, &adc_unit_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "adc_oneshot_new_unit failed (%d) - sliders disabled. If another module created the ADC unit, consider centralizing ADC init.", ret);
        s_enabled = false;
        return;
    }

#ifdef SLIDER_PB_ADC_CHANNEL
    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_11,
    };
    ret = adc_oneshot_config_channel(adc_unit_handle, (adc_channel_t)SLIDER_PB_ADC_CHANNEL, &chan_cfg);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "adc_oneshot_config_channel(PB) failed (%d). Check SLIDER_PB_ADC_CHANNEL macro is an adc_channel_t, not a GPIO number.", ret);
        s_enabled = false;
        return;
    }
#endif
#ifdef SLIDER_MOD_ADC_CHANNEL
    adc_oneshot_chan_cfg_t chan_cfg2 = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_11,
    };
    ret = adc_oneshot_config_channel(adc_unit_handle, (adc_channel_t)SLIDER_MOD_ADC_CHANNEL, &chan_cfg2);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "adc_oneshot_config_channel(MOD) failed (%d). Check SLIDER_MOD_ADC_CHANNEL macro.", ret);
    }
#endif
#ifdef SLIDER_VEL_ADC_CHANNEL
    adc_oneshot_chan_cfg_t chan_cfg3 = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_11,
    };
    ret = adc_oneshot_config_channel(adc_unit_handle, (adc_channel_t)SLIDER_VEL_ADC_CHANNEL, &chan_cfg3);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "adc_oneshot_config_channel(VEL) failed (%d). Check SLIDER_VEL_ADC_CHANNEL macro.", ret);
    }
#endif

    adc_initialized = true;
    s_enabled = true;
#else
    ESP_LOGW(TAG, "No SLIDER_* ADC channel macros defined; sliders disabled");
    s_enabled = false;
#endif
}

static uint16_t read_adc_channel_or_zero(adc_channel_t ch)
{
    if (!adc_initialized) return (uint16_t)s_last_raw;
    int raw = 0;
    esp_err_t ret = adc_oneshot_read(adc_unit_handle, ch, &raw);
    if (ret != ESP_OK) {
        s_adc_fail_count++;
        if (s_adc_fail_count == 1) {
            ESP_LOGW(TAG, "adc read failed for channel %d (first failure)", (int)ch);
        } else if (s_adc_fail_count == 8) {
            ESP_LOGW(TAG, "adc read failing repeatedly (%d times) - returning last good value", s_adc_fail_count);
        }
        /* return last good value (0..1023) */
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
#ifdef SLIDER_PB_ADC_CHANNEL
    if (!adc_initialized) return 0;

    /* Multi-sample with simple trimming to reject spikes */
#ifndef SLIDER_ADC_SAMPLES
#define SLIDER_ADC_SAMPLES 8
#endif
    const int samples = SLIDER_ADC_SAMPLES;
    int minv = INT_MAX;
    int maxv = INT_MIN;
    int sum = 0;
    for (int i = 0; i < samples; ++i) {
        int v = read_adc_channel_or_zero((adc_channel_t)SLIDER_PB_ADC_CHANNEL);
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
#else
    return 0;
#endif
}

uint16_t slider_read_mod(void)
{
#ifdef SLIDER_MOD_ADC_CHANNEL
    uint16_t v = read_adc_channel_or_zero((adc_channel_t)SLIDER_MOD_ADC_CHANNEL);
    float nv = (float)v;
    mod_ema = mod_ema * 0.9f + nv * 0.1f;
    return (uint16_t)mod_ema;
#else
    return 0;
#endif
}

uint16_t slider_read_velocity(void)
{
#ifdef SLIDER_VEL_ADC_CHANNEL
    uint16_t v = read_adc_channel_or_zero((adc_channel_t)SLIDER_VEL_ADC_CHANNEL);
    float nv = (float)v;
    vel_ema = vel_ema * 0.9f + nv * 0.1f;
    return (uint16_t)vel_ema;
#else
    return 0;
#endif
}

#else /* HAVE_ADC_ONESHOT */
/* Fallback: no modern ADC API available; keep previous stub behaviour */
void slider_init(void)
{
    ESP_LOGW(TAG, "ADC oneshot API not available; slider functions return 0");
}

uint16_t slider_read_pitchbend(void) { return 0; }
uint16_t slider_read_mod(void) { return 0; }
uint16_t slider_read_velocity(void) { return 0; }

#endif

bool slider_is_enabled(void)
{
#if HAVE_ADC_ONESHOT
    return s_enabled;
#else
    return false;
#endif
}
