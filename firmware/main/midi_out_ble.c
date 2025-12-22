#include "midi_out.h"

#include <stddef.h>

#include "esp_log.h"

#include "sdkconfig.h"

/* Defensive defaults for newly introduced Kconfig symbols.
 * This prevents build failures when the build directory has a stale sdkconfig.h.
 * Defaults must match Kconfig.projbuild.
 */
#ifndef CONFIG_EMIUET_MIDI_BLE_QUEUE_LEN
#define CONFIG_EMIUET_MIDI_BLE_QUEUE_LEN 256
#endif

#ifndef CONFIG_EMIUET_MIDI_TASK_BLE_PRIORITY
#define CONFIG_EMIUET_MIDI_TASK_BLE_PRIORITY 6
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "midi_out_ble";

/*
 * BLE-MIDI backend placeholder.
 *
 * This file provides the same non-blocking enqueue + sender-task shape as
 * TRS/USB so the rest of the firmware never blocks on I/O.
 * Actual BLE-MIDI transport will be implemented later.
 */

typedef struct {
    uint8_t len;
    uint8_t bytes[3];
} midi_tx_item_t;

static bool s_inited = false;
static QueueHandle_t s_ble_q = NULL;
static TaskHandle_t s_ble_tx_task = NULL;
static portMUX_TYPE s_ble_coalesce_mux = portMUX_INITIALIZER_UNLOCKED;

static bool s_ble_pb_pending[16] = {0};
static uint8_t s_ble_pb_lsb[16] = {0};
static uint8_t s_ble_pb_msb[16] = {0};

static bool s_ble_cc1_pending[16] = {0};
static uint8_t s_ble_cc1_val[16] = {0};

static uint32_t s_ble_drop_queue = 0;
static uint32_t s_ble_drop_send = 0;
static uint32_t s_ble_coalesce_pb = 0;
static uint32_t s_ble_coalesce_cc1 = 0;
static uint32_t s_ble_q_hwm = 0;
static TickType_t s_ble_last_stats_log_tick = 0;

static inline void ble_maybe_update_hwm(void)
{
    if (!s_ble_q) return;
    const UBaseType_t used = uxQueueMessagesWaiting(s_ble_q);
    if (used > s_ble_q_hwm) s_ble_q_hwm = (uint32_t)used;
}

static void ble_maybe_log_stats(void)
{
    const TickType_t now = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(1000);
    if (s_ble_last_stats_log_tick != 0 && (now - s_ble_last_stats_log_tick) < interval) return;

    if (s_ble_drop_queue || s_ble_drop_send || s_ble_coalesce_pb || s_ble_coalesce_cc1) {
        ESP_LOGW(TAG,
                 "stats q_hwm=%lu drop{q=%lu send=%lu} coalesce{pb=%lu cc1=%lu}",
                 (unsigned long)s_ble_q_hwm,
                 (unsigned long)s_ble_drop_queue,
                 (unsigned long)s_ble_drop_send,
                 (unsigned long)s_ble_coalesce_pb,
                 (unsigned long)s_ble_coalesce_cc1);
    }
    s_ble_last_stats_log_tick = now;
}

static inline bool is_pitchbend_3(const uint8_t *b, size_t len)
{
    return (len == 3) && ((b[0] & 0xF0u) == 0xE0u);
}

static inline bool is_cc1_3(const uint8_t *b, size_t len)
{
    return (len == 3) && ((b[0] & 0xF0u) == 0xB0u) && ((b[1] & 0x7Fu) == 1u);
}

static bool ble_send_lowlevel(const uint8_t *bytes, size_t len)
{
    (void)bytes;
    (void)len;
    /* BLE transport not implemented yet.
     * Return true so the placeholder backend does not continuously increment
     * drop counters and spam stats.
     *
     * IMPORTANT: This means BLE route must remain disabled in normal operation
     * until the real transport is implemented.
     */
    return true;
}

static void ble_flush_coalesced_once(void)
{
    for (int ch = 0; ch < 16; ++ch) {
        bool pb = false;
        uint8_t pb_lsb = 0;
        uint8_t pb_msb = 0;

        portENTER_CRITICAL(&s_ble_coalesce_mux);
        pb = s_ble_pb_pending[ch];
        if (pb) {
            pb_lsb = s_ble_pb_lsb[ch];
            pb_msb = s_ble_pb_msb[ch];
            s_ble_pb_pending[ch] = false;
        }
        portEXIT_CRITICAL(&s_ble_coalesce_mux);

        if (pb) {
            uint8_t b[3] = {(uint8_t)(0xE0u | (uint8_t)ch), pb_lsb, pb_msb};
            if (!ble_send_lowlevel(b, sizeof(b))) {
                s_ble_drop_send++;
            }
        }

        bool cc1 = false;
        uint8_t cc1_v = 0;

        portENTER_CRITICAL(&s_ble_coalesce_mux);
        cc1 = s_ble_cc1_pending[ch];
        if (cc1) {
            cc1_v = s_ble_cc1_val[ch];
            s_ble_cc1_pending[ch] = false;
        }
        portEXIT_CRITICAL(&s_ble_coalesce_mux);

        if (cc1) {
            uint8_t b[3] = {(uint8_t)(0xB0u | (uint8_t)ch), 1u, (uint8_t)(cc1_v & 0x7Fu)};
            if (!ble_send_lowlevel(b, sizeof(b))) {
                s_ble_drop_send++;
            }
        }
    }
}

static void ble_tx_task(void *arg)
{
    (void)arg;
    const int FLUSH_EVERY_N_EVENTS = 16;
    int sent_since_flush = 0;

    while (1) {
        midi_tx_item_t item = {0};
        if (s_ble_q && xQueueReceive(s_ble_q, &item, pdMS_TO_TICKS(10)) == pdTRUE) {
            ble_maybe_update_hwm();
            if (!ble_send_lowlevel(item.bytes, item.len)) {
                s_ble_drop_send++;
            } else {
                sent_since_flush++;
            }

            if (sent_since_flush >= FLUSH_EVERY_N_EVENTS) {
                sent_since_flush = 0;
                ble_flush_coalesced_once();
            }

            ble_maybe_log_stats();
            continue;
        }

        ble_flush_coalesced_once();
        ble_maybe_log_stats();
        taskYIELD();
    }
}

bool midi_out_ble_init(void)
{
    if (s_inited) return true;

    /* Keep BLE transport stubbed for now; we still set up the non-blocking path. */
    ESP_LOGI(TAG, "BLE-MIDI transport not implemented yet (stub)");

    if (s_ble_q == NULL) {
        s_ble_q = xQueueCreate(CONFIG_EMIUET_MIDI_BLE_QUEUE_LEN, sizeof(midi_tx_item_t));
        if (s_ble_q == NULL) {
            ESP_LOGW(TAG, "failed to create BLE queue");
            return false;
        }
    }

    if (s_ble_tx_task == NULL) {
        BaseType_t ok = xTaskCreatePinnedToCore(ble_tx_task,
                                               "midi_ble_tx",
                                               4096,
                                               NULL,
                                               CONFIG_EMIUET_MIDI_TASK_BLE_PRIORITY,
                                               &s_ble_tx_task,
                                               0);
        if (ok != pdPASS) {
            s_ble_tx_task = NULL;
            ESP_LOGW(TAG, "failed to create BLE sender task");
            return false;
        }
    }

    s_inited = true;
    return true;
}

bool midi_out_ble_send_bytes(const uint8_t *bytes, size_t len)
{
    if (!s_inited) return false;
    if (!bytes || len == 0) return false;

    if (is_pitchbend_3(bytes, len)) {
        const uint8_t ch = (uint8_t)(bytes[0] & 0x0Fu);
        portENTER_CRITICAL(&s_ble_coalesce_mux);
        if (s_ble_pb_pending[ch]) s_ble_coalesce_pb++;
        s_ble_pb_pending[ch] = true;
        s_ble_pb_lsb[ch] = (uint8_t)(bytes[1] & 0x7Fu);
        s_ble_pb_msb[ch] = (uint8_t)(bytes[2] & 0x7Fu);
        portEXIT_CRITICAL(&s_ble_coalesce_mux);
        return true;
    }

    if (is_cc1_3(bytes, len)) {
        const uint8_t ch = (uint8_t)(bytes[0] & 0x0Fu);
        portENTER_CRITICAL(&s_ble_coalesce_mux);
        if (s_ble_cc1_pending[ch]) s_ble_coalesce_cc1++;
        s_ble_cc1_pending[ch] = true;
        s_ble_cc1_val[ch] = (uint8_t)(bytes[2] & 0x7Fu);
        portEXIT_CRITICAL(&s_ble_coalesce_mux);
        return true;
    }

    if (!s_ble_q) return false;
    midi_tx_item_t item = {0};
    item.len = (uint8_t)((len > 3) ? 3 : len);
    for (size_t i = 0; i < item.len; ++i) item.bytes[i] = bytes[i];

    if (xQueueSendToBack(s_ble_q, &item, 0) != pdTRUE) {
        s_ble_drop_queue++;
        ble_maybe_update_hwm();
        ble_maybe_log_stats();
        return false;
    }

    ble_maybe_update_hwm();
    return true;
}
