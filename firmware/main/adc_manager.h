#pragma once

#include <stdbool.h>

#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Centralized ADC ownership for Emiuet.
 *
 * Rationale:
 * - Only this module creates adc_oneshot unit handles.
 * - Other modules (slider, OLED, etc.) only request reads.
 * - Eliminates boot-order dependent failures ("adc1 already in use").
 */

bool adc_manager_init(void);
bool adc_manager_is_enabled(void);

/* Read raw ADC code for a given GPIO (ADC-capable pin).
 * Uses adc_oneshot_io_to_channel() to map GPIO -> (unit, channel).
 */
esp_err_t adc_manager_read_raw(gpio_num_t gpio, int *out_raw);

/* Read millivolts for a given GPIO.
 * Uses ADC calibration if available, otherwise a linear approximation.
 */
esp_err_t adc_manager_read_mv(gpio_num_t gpio, int *out_mv);

#ifdef __cplusplus
}
#endif
