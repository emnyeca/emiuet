#include "adc_manager.h"

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

static const char *TAG = "adc_manager";

static SemaphoreHandle_t s_lock;
static bool s_inited;
static bool s_enabled;

static adc_oneshot_unit_handle_t s_unit1;
static adc_oneshot_unit_handle_t s_unit2;

static adc_cali_handle_t s_cali1;
static adc_cali_handle_t s_cali2;
static bool s_cali1_ok;
static bool s_cali2_ok;

/* Track which channels have been configured per unit.
 * Channel indices are small (<= 9 on ESP32-S3), so a bitmask works.
 */
static uint32_t s_cfg_mask_unit1;
static uint32_t s_cfg_mask_unit2;

static adc_oneshot_unit_handle_t unit_handle_for(adc_unit_t unit)
{
    switch (unit) {
    case ADC_UNIT_1:
        return s_unit1;
    case ADC_UNIT_2:
        return s_unit2;
    default:
        return NULL;
    }
}

static bool ensure_channel_configured_locked(adc_unit_t unit, adc_channel_t ch)
{
    adc_oneshot_unit_handle_t handle = unit_handle_for(unit);
    if (!handle) return false;

    uint32_t *mask = (unit == ADC_UNIT_1) ? &s_cfg_mask_unit1 : &s_cfg_mask_unit2;
    if ((uint32_t)ch < 32U && ((*mask) & (1U << (uint32_t)ch))) {
        return true;
    }

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };

    esp_err_t err = adc_oneshot_config_channel(handle, ch, &chan_cfg);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "adc_oneshot_config_channel(unit=%d, ch=%d) failed: %s", (int)unit, (int)ch, esp_err_to_name(err));
        return false;
    }

    if ((uint32_t)ch < 32U) {
        (*mask) |= (1U << (uint32_t)ch);
    }
    return true;
}

static void try_init_cali_for_unit(adc_unit_t unit, adc_cali_handle_t *out_handle, bool *out_ok)
{
    *out_ok = false;
    *out_handle = NULL;

    adc_cali_curve_fitting_config_t cfg = {
        .unit_id = unit,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    adc_cali_handle_t cali = NULL;
    if (adc_cali_create_scheme_curve_fitting(&cfg, &cali) == ESP_OK) {
        *out_handle = cali;
        *out_ok = true;
    }
}

bool adc_manager_init(void)
{
    if (s_inited) return s_enabled;

    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) {
        s_inited = true;
        s_enabled = false;
        ESP_LOGE(TAG, "mutex alloc failed");
        return false;
    }

    adc_oneshot_unit_init_cfg_t init1 = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    adc_oneshot_unit_init_cfg_t init2 = {
        .unit_id = ADC_UNIT_2,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };

    esp_err_t err1 = adc_oneshot_new_unit(&init1, &s_unit1);
    esp_err_t err2 = adc_oneshot_new_unit(&init2, &s_unit2);

    if (err1 != ESP_OK || err2 != ESP_OK) {
        ESP_LOGW(TAG, "adc_oneshot_new_unit failed (unit1=%s, unit2=%s)", esp_err_to_name(err1), esp_err_to_name(err2));
        s_unit1 = NULL;
        s_unit2 = NULL;
        s_inited = true;
        s_enabled = false;
        return false;
    }

    try_init_cali_for_unit(ADC_UNIT_1, &s_cali1, &s_cali1_ok);
    try_init_cali_for_unit(ADC_UNIT_2, &s_cali2, &s_cali2_ok);

    s_inited = true;
    s_enabled = true;
    ESP_LOGI(TAG, "initialized (cali1=%d, cali2=%d)", (int)s_cali1_ok, (int)s_cali2_ok);
    return true;
}

bool adc_manager_is_enabled(void)
{
    return s_enabled;
}

esp_err_t adc_manager_read_raw(gpio_num_t gpio, int *out_raw)
{
    if (!out_raw) return ESP_ERR_INVALID_ARG;
    if (!s_inited) {
        (void)adc_manager_init();
    }
    if (!s_enabled) return ESP_ERR_INVALID_STATE;

    adc_unit_t unit;
    adc_channel_t ch;
    esp_err_t err = adc_oneshot_io_to_channel((int)gpio, &unit, &ch);
    if (err != ESP_OK) {
        return err;
    }

    if (xSemaphoreTake(s_lock, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    bool ok = ensure_channel_configured_locked(unit, ch);
    if (!ok) {
        xSemaphoreGive(s_lock);
        return ESP_FAIL;
    }

    adc_oneshot_unit_handle_t handle = unit_handle_for(unit);
    int raw = 0;
    err = adc_oneshot_read(handle, ch, &raw);
    xSemaphoreGive(s_lock);

    if (err != ESP_OK) {
        return err;
    }

    *out_raw = raw;
    return ESP_OK;
}

esp_err_t adc_manager_read_mv(gpio_num_t gpio, int *out_mv)
{
    if (!out_mv) return ESP_ERR_INVALID_ARG;

    adc_unit_t unit;
    adc_channel_t ch;
    esp_err_t err = adc_oneshot_io_to_channel((int)gpio, &unit, &ch);
    if (err != ESP_OK) {
        return err;
    }

    int raw = 0;
    err = adc_manager_read_raw(gpio, &raw);
    if (err != ESP_OK) {
        return err;
    }

    int mv = 0;
    adc_cali_handle_t cali = (unit == ADC_UNIT_1) ? s_cali1 : s_cali2;
    bool cali_ok = (unit == ADC_UNIT_1) ? s_cali1_ok : s_cali2_ok;

    if (cali_ok) {
        err = adc_cali_raw_to_voltage(cali, raw, &mv);
        if (err == ESP_OK) {
            *out_mv = mv;
            return ESP_OK;
        }
    }

    /* Fallback approximation: 12-bit raw to 0..3300mV.
     * (Calibration is recommended for accuracy.)
     */
    if (raw < 0) raw = 0;
    if (raw > 4095) raw = 4095;
    mv = (raw * 3300) / 4095;
    *out_mv = mv;
    return ESP_OK;
}
