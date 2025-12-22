#include "midi_out.h"

#include <stddef.h>

#include "esp_log.h"

/* Defensive defaults for newly introduced Kconfig symbols.
 * This prevents build failures when the build directory has a stale sdkconfig.h.
 * Defaults must match Kconfig.projbuild.
 */
#ifndef CONFIG_EMIUET_MIDI_USB_QUEUE_LEN
#define CONFIG_EMIUET_MIDI_USB_QUEUE_LEN 1024
#endif

#ifndef CONFIG_EMIUET_MIDI_TASK_USB_PRIORITY
#define CONFIG_EMIUET_MIDI_TASK_USB_PRIORITY 6
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

static const char *TAG = "midi_out_usb";

/*
 * USB-MIDI backend
 *
 * This is intentionally isolated behind a config gate so the project
 * still builds even when TinyUSB is not enabled in sdkconfig.
 */
#if defined(CONFIG_TINYUSB_MIDI_COUNT) && (CONFIG_TINYUSB_MIDI_COUNT > 0)

#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tusb.h"
#include "class/midi/midi_device.h"

/* Some TinyUSB versions (including the one bundled via ESP-IDF 5.3.x managed component)
 * don't provide tud_midi_ready(). Provide a local shim so we can log the state using the
 * name requested, without changing descriptors or the send path.
 */
static inline bool tud_midi_ready(void)
{
    return tud_midi_mounted();
}

#ifndef EMUIET_USB_MIDI_VID
#define EMUIET_USB_MIDI_VID 0x303A /* Espressif VID (commonly used in examples) */
#endif

#ifndef EMUIET_USB_MIDI_PID
#define EMUIET_USB_MIDI_PID 0x4005
#endif

#ifndef EMUIET_USB_MIDI_BCD
#define EMUIET_USB_MIDI_BCD 0x0100
#endif

/* USB descriptors */
static const tusb_desc_device_t s_desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,

    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,

    /* 64B is valid for Full Speed EP0 and avoids relying on tusb_config.h */
    .bMaxPacketSize0 = 64,

    .idVendor = EMUIET_USB_MIDI_VID,
    .idProduct = EMUIET_USB_MIDI_PID,
    .bcdDevice = EMUIET_USB_MIDI_BCD,

    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,

    .bNumConfigurations = 0x01,
};

/* Strings must remain in flash/ROM */
static const char *s_string_desc[] = {
    (const char[]){0x09, 0x04}, /* 0: English (0x0409) */
    "Emnyeca",
    "Emiuet USB-MIDI",
    "0001",
};

/*
 * Configuration descriptor:
 * - One configuration
 * - One MIDI interface (TinyUSB MIDI class)
 */
#define EMUIET_USB_ITF_NUM_MIDI 0
#define EMUIET_USB_ITF_NUM_TOTAL 1

#define EMUIET_USB_EP_MIDI_OUT 0x01
#define EMUIET_USB_EP_MIDI_IN  0x81

#ifndef EMUIET_USB_MIDI_EP_SIZE
#define EMUIET_USB_MIDI_EP_SIZE 64
#endif

static const uint8_t s_desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, EMUIET_USB_ITF_NUM_TOTAL, 0, (TUD_CONFIG_DESC_LEN + TUD_MIDI_DESC_LEN), 0x00, 100),
    /* Note: TUD_MIDI_DESCRIPTOR signature depends on TinyUSB version. In esp-idf v5.3.4's tinyusb, it is:
     *   TUD_MIDI_DESCRIPTOR(itfnum, stridx, epout, epin, epsize)
     * We don't provide a dedicated interface string, so stridx=0.
     */
    TUD_MIDI_DESCRIPTOR(EMUIET_USB_ITF_NUM_MIDI, 0, EMUIET_USB_EP_MIDI_OUT, EMUIET_USB_EP_MIDI_IN, EMUIET_USB_MIDI_EP_SIZE)
};

static bool s_inited = false;
static TaskHandle_t s_usb_state_task_handle = NULL;

typedef struct {
    uint8_t len;
    uint8_t bytes[3];
} midi_tx_item_t;

static QueueHandle_t s_usb_q = NULL;
static TaskHandle_t s_usb_tx_task_handle = NULL;
static portMUX_TYPE s_usb_coalesce_mux = portMUX_INITIALIZER_UNLOCKED;

static bool s_usb_pb_pending[16] = {0};
static uint8_t s_usb_pb_lsb[16] = {0};
static uint8_t s_usb_pb_msb[16] = {0};

static bool s_usb_cc1_pending[16] = {0};
static uint8_t s_usb_cc1_val[16] = {0};

static uint32_t s_usb_drop_queue = 0;
static uint32_t s_usb_drop_write = 0;
static uint32_t s_usb_coalesce_pb = 0;
static uint32_t s_usb_coalesce_cc1 = 0;
static uint32_t s_usb_q_hwm = 0;
static TickType_t s_usb_last_stats_log_tick = 0;

static inline void usb_maybe_update_hwm(void)
{
    if (!s_usb_q) return;
    const UBaseType_t used = uxQueueMessagesWaiting(s_usb_q);
    if (used > s_usb_q_hwm) s_usb_q_hwm = (uint32_t)used;
}

static void usb_maybe_log_stats(void)
{
    const TickType_t now = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(1000);
    if (s_usb_last_stats_log_tick != 0 && (now - s_usb_last_stats_log_tick) < interval) return;

    if (s_usb_drop_queue || s_usb_drop_write || s_usb_coalesce_pb || s_usb_coalesce_cc1) {
        ESP_LOGW(TAG,
                 "stats q_hwm=%lu drop{q=%lu write=%lu} coalesce{pb=%lu cc1=%lu}",
                 (unsigned long)s_usb_q_hwm,
                 (unsigned long)s_usb_drop_queue,
                 (unsigned long)s_usb_drop_write,
                 (unsigned long)s_usb_coalesce_pb,
                 (unsigned long)s_usb_coalesce_cc1);
    }
    s_usb_last_stats_log_tick = now;
}

static inline bool is_pitchbend_3(const uint8_t *b, size_t len)
{
    return (len == 3) && ((b[0] & 0xF0u) == 0xE0u);
}

static inline bool is_cc1_3(const uint8_t *b, size_t len)
{
    return (len == 3) && ((b[0] & 0xF0u) == 0xB0u) && ((b[1] & 0x7Fu) == 1u);
}

static bool usb_send_lowlevel(const uint8_t *bytes, size_t len)
{
    if (!s_inited) return false;
    if (!bytes || len == 0) return false;
    if (!tud_mounted()) return false;

    uint32_t written = tud_midi_stream_write(0, bytes, (uint32_t)len);
    return written == len;
}

static void usb_flush_coalesced_once(void)
{
    for (int ch = 0; ch < 16; ++ch) {
        bool pb = false;
        uint8_t pb_lsb = 0;
        uint8_t pb_msb = 0;

        portENTER_CRITICAL(&s_usb_coalesce_mux);
        pb = s_usb_pb_pending[ch];
        if (pb) {
            pb_lsb = s_usb_pb_lsb[ch];
            pb_msb = s_usb_pb_msb[ch];
            s_usb_pb_pending[ch] = false;
        }
        portEXIT_CRITICAL(&s_usb_coalesce_mux);

        if (pb) {
            uint8_t b[3] = {(uint8_t)(0xE0u | (uint8_t)ch), pb_lsb, pb_msb};
            if (!usb_send_lowlevel(b, sizeof(b))) {
                /* Keep pending on failure to avoid losing latest value. */
                portENTER_CRITICAL(&s_usb_coalesce_mux);
                s_usb_pb_pending[ch] = true;
                s_usb_pb_lsb[ch] = pb_lsb;
                s_usb_pb_msb[ch] = pb_msb;
                portEXIT_CRITICAL(&s_usb_coalesce_mux);
                s_usb_drop_write++;
                return;
            }
        }

        bool cc1 = false;
        uint8_t cc1_v = 0;

        portENTER_CRITICAL(&s_usb_coalesce_mux);
        cc1 = s_usb_cc1_pending[ch];
        if (cc1) {
            cc1_v = s_usb_cc1_val[ch];
            s_usb_cc1_pending[ch] = false;
        }
        portEXIT_CRITICAL(&s_usb_coalesce_mux);

        if (cc1) {
            uint8_t b[3] = {(uint8_t)(0xB0u | (uint8_t)ch), 1u, (uint8_t)(cc1_v & 0x7Fu)};
            if (!usb_send_lowlevel(b, sizeof(b))) {
                portENTER_CRITICAL(&s_usb_coalesce_mux);
                s_usb_cc1_pending[ch] = true;
                s_usb_cc1_val[ch] = cc1_v;
                portEXIT_CRITICAL(&s_usb_coalesce_mux);
                s_usb_drop_write++;
                return;
            }
        }
    }
}

static void midi_out_usb_tx_task(void *arg)
{
    (void)arg;
    const int FLUSH_EVERY_N_EVENTS = 16;
    int sent_since_flush = 0;

    while (1) {
        if (!tud_mounted()) {
            /* Not mounted: keep queued discrete events and latest coalesced values. */
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        /* Discrete events: peek+send+pop so we don't drop on transient failure. */
        midi_tx_item_t item = {0};
        if (s_usb_q && xQueuePeek(s_usb_q, &item, pdMS_TO_TICKS(1)) == pdTRUE) {
            usb_maybe_update_hwm();
            if (usb_send_lowlevel(item.bytes, item.len)) {
                (void)xQueueReceive(s_usb_q, &item, 0);
                sent_since_flush++;
            } else {
                s_usb_drop_write++;
                usb_maybe_log_stats();
                vTaskDelay(pdMS_TO_TICKS(1));
                continue;
            }

            if (sent_since_flush >= FLUSH_EVERY_N_EVENTS) {
                sent_since_flush = 0;
                usb_flush_coalesced_once();
            }

            usb_maybe_log_stats();
            continue;
        }

        /* Idle path */
        usb_flush_coalesced_once();
        usb_maybe_log_stats();
        taskYIELD();
    }
}

static void midi_out_usb_event_cb(tinyusb_event_t *event, void *arg)
{
    (void)arg;
    if (!event) return;

    switch (event->id) {
        case TINYUSB_EVENT_ATTACHED:
            ESP_LOGI(TAG, "tud_mount_cb(): tud_mounted()=%d tud_midi_ready()=%d", (int)tud_mounted(), (int)tud_midi_ready());
            break;
        case TINYUSB_EVENT_DETACHED:
            ESP_LOGI(TAG, "tud_umount_cb(): tud_mounted()=%d tud_midi_ready()=%d", (int)tud_mounted(), (int)tud_midi_ready());
            break;
        default:
            break;
    }
}

static void midi_out_usb_state_task(void *arg)
{
    (void)arg;

    bool last_mounted = false;
    bool last_ready = false;

    while (1) {
        const bool mounted = tud_mounted();
        const bool ready = mounted && tud_midi_ready();

        if (mounted != last_mounted) {
            ESP_LOGI(TAG, "tud_mounted() -> %d", (int)mounted);
            last_mounted = mounted;
        }

        if (ready != last_ready) {
            ESP_LOGI(TAG, "tud_midi_ready() -> %d", (int)ready);
            last_ready = ready;
        }

        /* Short delay to avoid busy looping.
         * Note: the actual TinyUSB stack is serviced by esp_tinyusb's own task (tud_task()).
         */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

bool midi_out_usb_init(void)
{
    if (s_inited) return true;

    /* TODO (prototype bring-up):
     * On some ESP32-S3 DevKits, the USB connector used for flashing/monitoring is USB-Serial/JTAG,
      * not the native USB OTG D+/D- (PIN_USB_D_PLUS/PIN_USB_D_MINUS). In that case Windows won't enumerate this
     * TinyUSB MIDI device and tud_mount_cb()/tud_mounted() won't fire.
     * Ensure the cable is on the native OTG port (or temporarily disable USB-Serial/JTAG) when
     * validating USB-MIDI enumeration.
     */

    /* Default config provides sane task/PHY defaults; we override descriptors for MIDI. */
    tinyusb_config_t cfg = TINYUSB_DEFAULT_CONFIG();
    cfg.descriptor.device = &s_desc_device;
    cfg.descriptor.string = s_string_desc;
    cfg.descriptor.full_speed_config = s_desc_configuration;
#if (TUD_OPT_HIGH_SPEED)
    cfg.descriptor.high_speed_config = s_desc_configuration;
#endif

    /* Ensure the TinyUSB stack task (which runs tud_task() in a loop) is pinned to CPU0.
     * This keeps USB enumeration progressing even after app_main() returns.
     */
    cfg.task.xCoreID = 0;
    cfg.task.priority = 5;
    cfg.event_cb = midi_out_usb_event_cb;
    cfg.event_arg = NULL;

    esp_err_t err = tinyusb_driver_install(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "tinyusb_driver_install failed: %s", esp_err_to_name(err));
        return false;
    }

    if (s_usb_state_task_handle == NULL) {
        BaseType_t ok = xTaskCreatePinnedToCore(midi_out_usb_state_task,
                                               "usb_state",
                                               2048,
                                               NULL,
                                               5,
                                               &s_usb_state_task_handle,
                                               0);
        if (ok != pdPASS) {
            s_usb_state_task_handle = NULL;
            ESP_LOGW(TAG, "Failed to start USB state monitor task");
        }
    }

    s_inited = true;
    ESP_LOGI(TAG, "USB-MIDI backend initialized");

    if (s_usb_q == NULL) {
        s_usb_q = xQueueCreate(CONFIG_EMIUET_MIDI_USB_QUEUE_LEN, sizeof(midi_tx_item_t));
        if (s_usb_q == NULL) {
            ESP_LOGE(TAG, "failed to create USB MIDI queue");
            return true;
        }
    }

    if (s_usb_tx_task_handle == NULL) {
        BaseType_t ok = xTaskCreatePinnedToCore(midi_out_usb_tx_task,
                                               "midi_usb_tx",
                                               4096,
                                               NULL,
                                               CONFIG_EMIUET_MIDI_TASK_USB_PRIORITY,
                                               &s_usb_tx_task_handle,
                                               0);
        if (ok != pdPASS) {
            s_usb_tx_task_handle = NULL;
            ESP_LOGW(TAG, "failed to create USB MIDI sender task");
        }
    }

    return true;
}

bool midi_out_usb_send_bytes(const uint8_t *bytes, size_t len)
{
    if (!s_inited) return false;
    if (!bytes || len == 0) return false;

    /* Coalesce continuous controllers to prevent queue saturation. */
    if (is_pitchbend_3(bytes, len)) {
        const uint8_t ch = (uint8_t)(bytes[0] & 0x0Fu);
        portENTER_CRITICAL(&s_usb_coalesce_mux);
        if (s_usb_pb_pending[ch]) s_usb_coalesce_pb++;
        s_usb_pb_pending[ch] = true;
        s_usb_pb_lsb[ch] = (uint8_t)(bytes[1] & 0x7Fu);
        s_usb_pb_msb[ch] = (uint8_t)(bytes[2] & 0x7Fu);
        portEXIT_CRITICAL(&s_usb_coalesce_mux);
        return true;
    }

    if (is_cc1_3(bytes, len)) {
        const uint8_t ch = (uint8_t)(bytes[0] & 0x0Fu);
        portENTER_CRITICAL(&s_usb_coalesce_mux);
        if (s_usb_cc1_pending[ch]) s_usb_coalesce_cc1++;
        s_usb_cc1_pending[ch] = true;
        s_usb_cc1_val[ch] = (uint8_t)(bytes[2] & 0x7Fu);
        portEXIT_CRITICAL(&s_usb_coalesce_mux);
        return true;
    }

    if (!s_usb_q) {
        /* If init didn't create queue for some reason, fall back to direct send. */
        return usb_send_lowlevel(bytes, len);
    }

    midi_tx_item_t item = {0};
    item.len = (uint8_t)((len > 3) ? 3 : len);
    for (size_t i = 0; i < item.len; ++i) item.bytes[i] = bytes[i];

    if (xQueueSendToBack(s_usb_q, &item, 0) != pdTRUE) {
        s_usb_drop_queue++;
        usb_maybe_update_hwm();
        usb_maybe_log_stats();
        return false;
    }

    usb_maybe_update_hwm();
    return true;
}

#else

bool midi_out_usb_init(void)
{
    /* TinyUSB not enabled in sdkconfig; keep build working. */
    ESP_LOGW(TAG, "TinyUSB MIDI not enabled (CONFIG_TINYUSB_MIDI_COUNT=0); USB-MIDI backend disabled");
    return false;
}

bool midi_out_usb_send_bytes(const uint8_t *bytes, size_t len)
{
    (void)bytes;
    (void)len;
    return false;
}

#endif
