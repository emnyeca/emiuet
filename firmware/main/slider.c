#include "slider.h"
#include "board_pins.h"
#include "esp_err.h"
#include "esp_log.h"

#if __has_include("esp_adc/adc_oneshot.h")
#include "esp_adc/adc_oneshot.h"
#define HAVE_ADC_ONESHOT 1
#else
#include "driver/adc.h"
#define HAVE_ADC_ONESHOT 0
#endif

static const char *TAG = "slider";

/* EMA filter state */
static float pb_ema = 0.0f;
static float mod_ema = 0.0f;
static float vel_ema = 0.0f;

#if HAVE_ADC_ONESHOT
static adc_oneshot_unit_handle_t adc_unit_handle = NULL;
static bool adc_initialized = false;

void slider_init(void)
{
    if (adc_initialized) return;

#if defined(SLIDER_PB_ADC_CHANNEL) || defined(SLIDER_MOD_ADC_CHANNEL) || defined(SLIDER_VEL_ADC_CHANNEL)
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc_unit_handle));

#ifdef SLIDER_PB_ADC_CHANNEL
    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_11,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_unit_handle, (adc_channel_t)SLIDER_PB_ADC_CHANNEL, &chan_cfg));
#endif
#ifdef SLIDER_MOD_ADC_CHANNEL
    adc_oneshot_chan_cfg_t chan_cfg2 = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_11,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_unit_handle, (adc_channel_t)SLIDER_MOD_ADC_CHANNEL, &chan_cfg2));
#endif
#ifdef SLIDER_VEL_ADC_CHANNEL
    adc_oneshot_chan_cfg_t chan_cfg3 = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_11,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_unit_handle, (adc_channel_t)SLIDER_VEL_ADC_CHANNEL, &chan_cfg3));
#endif

    adc_initialized = true;
#else
    ESP_LOGW(TAG, "No SLIDER_* ADC channel macros defined; sliders disabled");
#endif
}

static uint16_t read_adc_channel_or_zero(adc_channel_t ch)
{
    if (!adc_initialized) return 0;
    int raw = 0;
    esp_err_t ret = adc_oneshot_read(adc_unit_handle, ch, &raw);
    if (ret != ESP_OK) return 0;
    /* Map raw (0..4095) to 0..1023 */
    if (raw < 0) raw = 0;
    if (raw > 4095) raw = 4095;
    return (uint16_t)((raw * 1023) / 4095);
}

uint16_t slider_read_pitchbend(void)
{
#ifdef SLIDER_PB_ADC_CHANNEL
    uint16_t v = read_adc_channel_or_zero((adc_channel_t)SLIDER_PB_ADC_CHANNEL);
    float nv = (float)v;
    pb_ema = pb_ema * 0.9f + nv * 0.1f;
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
