#include "midi_out.h"

#include <stddef.h>

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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
    return true;
}

bool midi_out_usb_send_bytes(const uint8_t *bytes, size_t len)
{
    if (!s_inited) return false;
    if (!bytes || len == 0) return false;

    if (!tud_mounted()) {
        return false;
    }

    /* Stream write will packetize into USB-MIDI event packets internally. */
    uint32_t written = tud_midi_stream_write(0, bytes, (uint32_t)len);
    return written == len;
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
