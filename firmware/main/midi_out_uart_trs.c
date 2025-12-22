#include "midi_out.h"

#include <stddef.h>

#include "board_pins.h"
#include "sdkconfig.h"

#include "esp_log.h"

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "midi_out_uart_trs";

/*
 * TRS MIDI (Type-A) backend
 *
 * - UART: 31250 bps, 8-N-1
 * - Hardware is responsible for MIDI electrical compliance.
 *
 * NOTE: The design maps TRS MIDI OUT to PIN_MIDI_OUT_TX (UART0 TX).
 * If the ESP-IDF console also uses UART0, it will conflict.
 */

#define MIDI_TRS_UART_PORT       UART_NUM_0
#define MIDI_TRS_UART_BAUDRATE   31250

static bool s_inited = false;
static bool s_enabled = false;
static SemaphoreHandle_t s_mutex = NULL;

static uint32_t s_drop_mutex = 0;
static uint32_t s_drop_write = 0;
static TickType_t s_last_drop_log_tick = 0;

static void maybe_log_drops(void)
{
    const TickType_t now = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(1000);

    if (s_last_drop_log_tick == 0 || (now - s_last_drop_log_tick) >= interval) {
        if (s_drop_mutex || s_drop_write) {
            ESP_LOGW(TAG, "TRS UART drops: mutex=%lu write=%lu",
                     (unsigned long)s_drop_mutex,
                     (unsigned long)s_drop_write);
        }
        s_last_drop_log_tick = now;
    }
}

bool midi_out_uart_trs_init(void)
{
    if (s_inited) return s_enabled;
    s_inited = true;

#if !CONFIG_EMIUET_MIDI_TRS_UART_ENABLE
    s_enabled = false;
    ESP_LOGI(TAG, "TRS UART backend disabled (CONFIG_EMIUET_MIDI_TRS_UART_ENABLE=n)");
    return false;
#else

    /* Protect against the common default: console on UART0. */
#if defined(CONFIG_ESP_CONSOLE_UART) && CONFIG_ESP_CONSOLE_UART
#if defined(CONFIG_ESP_CONSOLE_UART_NUM) && (CONFIG_ESP_CONSOLE_UART_NUM == 0)
#if !CONFIG_EMIUET_MIDI_TRS_UART_ALLOW_UART0_CONSOLE_CONFLICT
    s_enabled = false;
    ESP_LOGE(TAG,
             "TRS UART backend not started: console uses UART0 (CONFIG_ESP_CONSOLE_UART_NUM=0). "
             "Move console off UART0 (e.g., USB Serial/JTAG) or set CONFIG_EMIUET_MIDI_TRS_UART_ALLOW_UART0_CONSOLE_CONFLICT=y.");
    return false;
#endif
#endif
#endif

    uart_config_t cfg = {
        .baud_rate = MIDI_TRS_UART_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_param_config(MIDI_TRS_UART_PORT, &cfg);
    if (err != ESP_OK) {
        s_enabled = false;
        ESP_LOGE(TAG, "uart_param_config failed: %s", esp_err_to_name(err));
        return false;
    }

    err = uart_set_pin(MIDI_TRS_UART_PORT,
                       (int)PIN_MIDI_OUT_TX,
                       UART_PIN_NO_CHANGE,
                       UART_PIN_NO_CHANGE,
                       UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        s_enabled = false;
        ESP_LOGE(TAG, "uart_set_pin failed: %s", esp_err_to_name(err));
        return false;
    }

    /* TX only. We keep a small TX buffer so callers can write without blocking. */
    err = uart_driver_install(MIDI_TRS_UART_PORT, 0, 256, 0, NULL, 0);
    if (err != ESP_OK) {
        s_enabled = false;
        ESP_LOGE(TAG, "uart_driver_install failed: %s", esp_err_to_name(err));
        return false;
    }

    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        s_enabled = false;
        ESP_LOGE(TAG, "failed to create mutex");
        return false;
    }

    s_enabled = true;
    ESP_LOGI(TAG, "TRS UART backend initialized (port=%d tx=%d baud=%d)",
             (int)MIDI_TRS_UART_PORT, (int)PIN_MIDI_OUT_TX, (int)MIDI_TRS_UART_BAUDRATE);
    return true;
#endif
}

bool midi_out_uart_trs_send_bytes(const uint8_t *bytes, size_t len)
{
    if (!s_enabled) return false;
    if (!bytes || len == 0) return false;

    if (s_mutex) {
        if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(5)) != pdTRUE) {
            s_drop_mutex++;
            maybe_log_drops();
            return false;
        }
    }

    int written = uart_write_bytes(MIDI_TRS_UART_PORT, (const char *)bytes, (size_t)len);
    if (written != (int)len) {
        s_drop_write++;
        maybe_log_drops();
    }

    if (s_mutex) {
        xSemaphoreGive(s_mutex);
    }

    return (written == (int)len);
}
