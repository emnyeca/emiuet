#include "midi_out.h"

#include <stddef.h>

#include "board_pins.h"
#include "sdkconfig.h"

/* Defensive defaults for newly introduced Kconfig symbols.
 * This prevents build failures when the build directory has a stale sdkconfig.h.
 * Defaults must match Kconfig.projbuild.
 */
#ifndef CONFIG_EMIUET_MIDI_TRS_UART_ENABLE
#define CONFIG_EMIUET_MIDI_TRS_UART_ENABLE 0
#endif

#ifndef CONFIG_EMIUET_MIDI_TRS_UART_ALLOW_UART0_CONSOLE_CONFLICT
#define CONFIG_EMIUET_MIDI_TRS_UART_ALLOW_UART0_CONSOLE_CONFLICT 0
#endif

#ifndef CONFIG_EMIUET_MIDI_TASK_TRS_PRIORITY
#define CONFIG_EMIUET_MIDI_TASK_TRS_PRIORITY 7
#endif

#ifndef CONFIG_EMIUET_MIDI_TRS_QUEUE_LEN
#define CONFIG_EMIUET_MIDI_TRS_QUEUE_LEN 64
#endif

#include "esp_log.h"

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

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

#define MIDI_TRS_COALESCE_CHANNELS 16

typedef struct {
    uint8_t len;
    uint8_t bytes[3];
} midi_tx_item_t;

static bool s_inited = false;
static bool s_enabled = false;
static QueueHandle_t s_q = NULL;
static TaskHandle_t s_task = NULL;
static portMUX_TYPE s_coalesce_mux = portMUX_INITIALIZER_UNLOCKED;

/* Coalesce state (per-channel) */
static bool s_pb_pending[MIDI_TRS_COALESCE_CHANNELS] = {0};
static uint8_t s_pb_lsb[MIDI_TRS_COALESCE_CHANNELS] = {0};
static uint8_t s_pb_msb[MIDI_TRS_COALESCE_CHANNELS] = {0};

static bool s_cc1_pending[MIDI_TRS_COALESCE_CHANNELS] = {0};
static uint8_t s_cc1_val[MIDI_TRS_COALESCE_CHANNELS] = {0};

/* Stats */
static uint32_t s_drop_queue = 0;
static uint32_t s_drop_write = 0;
static uint32_t s_coalesce_pb = 0;
static uint32_t s_coalesce_cc1 = 0;
static uint32_t s_q_hwm = 0;
static TickType_t s_last_stats_log_tick = 0;

static inline void maybe_update_hwm(void)
{
    if (!s_q) return;
    const UBaseType_t used = uxQueueMessagesWaiting(s_q);
    if (used > s_q_hwm) s_q_hwm = (uint32_t)used;
}

static void maybe_log_stats(void)
{
    const TickType_t now = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(1000);
    if (s_last_stats_log_tick != 0 && (now - s_last_stats_log_tick) < interval) return;

    if (s_drop_queue || s_drop_write || s_coalesce_pb || s_coalesce_cc1) {
        ESP_LOGW(TAG,
                 "stats q_hwm=%lu drop{q=%lu write=%lu} coalesce{pb=%lu cc1=%lu}",
                 (unsigned long)s_q_hwm,
                 (unsigned long)s_drop_queue,
                 (unsigned long)s_drop_write,
                 (unsigned long)s_coalesce_pb,
                 (unsigned long)s_coalesce_cc1);
    }
    s_last_stats_log_tick = now;
}

static inline bool is_pitchbend_3(const uint8_t *b, size_t len)
{
    return (len == 3) && ((b[0] & 0xF0u) == 0xE0u);
}

static inline bool is_cc1_3(const uint8_t *b, size_t len)
{
    return (len == 3) && ((b[0] & 0xF0u) == 0xB0u) && ((b[1] & 0x7Fu) == 1u);
}

static bool trs_uart_write_bytes(const uint8_t *bytes, size_t len)
{
    if (!bytes || len == 0) return false;
    int written = uart_write_bytes(MIDI_TRS_UART_PORT, (const char *)bytes, len);
    return written == (int)len;
}

static void trs_flush_coalesced_once(void)
{
    /* Flush coalesced continuous values. We keep this bounded and quick. */
    for (int ch = 0; ch < MIDI_TRS_COALESCE_CHANNELS; ++ch) {
        bool pb = false;
        uint8_t pb_lsb = 0;
        uint8_t pb_msb = 0;

        portENTER_CRITICAL(&s_coalesce_mux);
        pb = s_pb_pending[ch];
        if (pb) {
            pb_lsb = s_pb_lsb[ch];
            pb_msb = s_pb_msb[ch];
            s_pb_pending[ch] = false;
        }
        portEXIT_CRITICAL(&s_coalesce_mux);

        if (pb) {
            uint8_t b[3] = {(uint8_t)(0xE0u | (uint8_t)ch), pb_lsb, pb_msb};
            if (!trs_uart_write_bytes(b, sizeof(b))) {
                s_drop_write++;
            }
        }

        bool cc1 = false;
        uint8_t cc1_v = 0;

        portENTER_CRITICAL(&s_coalesce_mux);
        cc1 = s_cc1_pending[ch];
        if (cc1) {
            cc1_v = s_cc1_val[ch];
            s_cc1_pending[ch] = false;
        }
        portEXIT_CRITICAL(&s_coalesce_mux);

        if (cc1) {
            uint8_t b[3] = {(uint8_t)(0xB0u | (uint8_t)ch), 1u, (uint8_t)(cc1_v & 0x7Fu)};
            if (!trs_uart_write_bytes(b, sizeof(b))) {
                s_drop_write++;
            }
        }
    }
}

static void trs_sender_task(void *arg)
{
    (void)arg;

    /* We intentionally do not call uart_wait_tx_done() per message.
     * UART driver TX buffer + this dedicated task provides stable latency.
     */
    const int FLUSH_EVERY_N_EVENTS = 8;
    int sent_since_flush = 0;

    while (1) {
        midi_tx_item_t item = {0};

        /* Wait for discrete events; if none arrive, periodically flush coalesced values. */
        if (s_q && xQueueReceive(s_q, &item, pdMS_TO_TICKS(1)) == pdTRUE) {
            maybe_update_hwm();

            /* Single sender task owns the UART; no mutex required. */
            bool ok = trs_uart_write_bytes(item.bytes, item.len);

            if (!ok) {
                s_drop_write++;
            }

            sent_since_flush++;
            if (sent_since_flush >= FLUSH_EVERY_N_EVENTS) {
                sent_since_flush = 0;
                trs_flush_coalesced_once();
            }

            maybe_log_stats();
            continue;
        }

        /* Idle path: flush continuous updates promptly. */
        trs_flush_coalesced_once();
        maybe_log_stats();

        taskYIELD();
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
    /* Provide RX buffer too (even if unused) to avoid edge-case behavior differences
     * across ESP-IDF versions/configs.
     */
    err = uart_driver_install(MIDI_TRS_UART_PORT, 256, 512, 0, NULL, 0);
    if (err != ESP_OK) {
        s_enabled = false;
        ESP_LOGE(TAG, "uart_driver_install failed: %s", esp_err_to_name(err));
        return false;
    }

    s_q = xQueueCreate(CONFIG_EMIUET_MIDI_TRS_QUEUE_LEN, sizeof(midi_tx_item_t));
    if (s_q == NULL) {
        s_enabled = false;
        ESP_LOGE(TAG, "failed to create queue");
        return false;
    }

    if (s_task == NULL) {
        BaseType_t ok = xTaskCreatePinnedToCore(trs_sender_task,
                                               "midi_trs_tx",
                                               4096,
                                               NULL,
                                               CONFIG_EMIUET_MIDI_TASK_TRS_PRIORITY,
                                               &s_task,
                                               0);
        if (ok != pdPASS) {
            s_task = NULL;
            s_enabled = false;
            ESP_LOGE(TAG, "failed to create sender task");
            return false;
        }
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

    /* Coalesce continuous controllers to prevent queue saturation. */
    if (is_pitchbend_3(bytes, len)) {
        const uint8_t ch = (uint8_t)(bytes[0] & 0x0Fu);
        portENTER_CRITICAL(&s_coalesce_mux);
        if (s_pb_pending[ch]) s_coalesce_pb++;
        s_pb_pending[ch] = true;
        s_pb_lsb[ch] = (uint8_t)(bytes[1] & 0x7Fu);
        s_pb_msb[ch] = (uint8_t)(bytes[2] & 0x7Fu);
        portEXIT_CRITICAL(&s_coalesce_mux);
        return true;
    }

    if (is_cc1_3(bytes, len)) {
        const uint8_t ch = (uint8_t)(bytes[0] & 0x0Fu);
        portENTER_CRITICAL(&s_coalesce_mux);
        if (s_cc1_pending[ch]) s_coalesce_cc1++;
        s_cc1_pending[ch] = true;
        s_cc1_val[ch] = (uint8_t)(bytes[2] & 0x7Fu);
        portEXIT_CRITICAL(&s_coalesce_mux);
        return true;
    }

    if (!s_q) return false;
    midi_tx_item_t item = {0};
    item.len = (uint8_t)((len > 3) ? 3 : len);
    for (size_t i = 0; i < item.len; ++i) item.bytes[i] = bytes[i];

    if (xQueueSendToBack(s_q, &item, 0) != pdTRUE) {
        s_drop_queue++;
        maybe_update_hwm();
        maybe_log_stats();
        return false;
    }

    maybe_update_hwm();
    return true;
}
