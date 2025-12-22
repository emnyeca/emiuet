#include "midi_out.h"

#include <stddef.h>

#include "esp_log.h"

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
    TUD_MIDI_DESCRIPTOR(EMUIET_USB_ITF_NUM_MIDI, 4, EMUIET_USB_EP_MIDI_OUT, EMUIET_USB_MIDI_EP_SIZE, EMUIET_USB_EP_MIDI_IN, EMUIET_USB_MIDI_EP_SIZE)
};

static bool s_inited = false;

bool midi_out_usb_init(void)
{
    if (s_inited) return true;

    /* Default config provides sane task/PHY defaults; we override descriptors for MIDI. */
    tinyusb_config_t cfg = TINYUSB_DEFAULT_CONFIG();
    cfg.descriptor.device = &s_desc_device;
    cfg.descriptor.string = s_string_desc;
    cfg.descriptor.full_speed_config = s_desc_configuration;
#if (TUD_OPT_HIGH_SPEED)
    cfg.descriptor.high_speed_config = s_desc_configuration;
#endif

    esp_err_t err = tinyusb_driver_install(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "tinyusb_driver_install failed: %s", esp_err_to_name(err));
        return false;
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
