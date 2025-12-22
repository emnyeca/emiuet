#include "midi_out.h"

#include <stddef.h>
#include <string.h>

#include "esp_log.h"
#include "sdkconfig.h"

/* Backends */
bool midi_out_usb_init(void);
bool midi_out_usb_send_bytes(const uint8_t *bytes, size_t len);

bool midi_out_uart_trs_init(void);
bool midi_out_uart_trs_send_bytes(const uint8_t *bytes, size_t len);

bool midi_out_ble_init(void);
bool midi_out_ble_send_bytes(const uint8_t *bytes, size_t len);

static const char *TAG = "midi_out";

static bool s_inited = false;
static uint32_t s_routes = MIDI_OUT_ROUTE_USB; /* default: USB only */

static inline uint8_t clamp_ch(uint8_t ch) { return (ch > 15) ? 15 : ch; }

static bool send_bytes_to_routes(uint32_t routes, const uint8_t *bytes, size_t len)
{
    bool ok = false;
    if ((routes & MIDI_OUT_ROUTE_USB) != 0) {
        ok |= midi_out_usb_send_bytes(bytes, len);
    }
    if ((routes & MIDI_OUT_ROUTE_TRS_UART) != 0) {
        ok |= midi_out_uart_trs_send_bytes(bytes, len);
    }
    if ((routes & MIDI_OUT_ROUTE_BLE) != 0) {
        ok |= midi_out_ble_send_bytes(bytes, len);
    }
    return ok;
}

static bool encode_and_send(const midi_msg_t *msg)
{
    if (!msg) return false;

    uint8_t ch = clamp_ch(msg->channel);
    uint8_t bytes[3] = {0};
    size_t len = 0;

    switch (msg->type) {
        case MIDI_MSG_NOTE_ON:
            bytes[0] = (uint8_t)(0x90u | ch);
            bytes[1] = msg->data.note.note & 0x7Fu;
            bytes[2] = msg->data.note.velocity & 0x7Fu;
            len = 3;
            break;

        case MIDI_MSG_NOTE_OFF:
            bytes[0] = (uint8_t)(0x80u | ch);
            bytes[1] = msg->data.note.note & 0x7Fu;
            bytes[2] = msg->data.note.velocity & 0x7Fu;
            len = 3;
            break;

        case MIDI_MSG_CC:
            bytes[0] = (uint8_t)(0xB0u | ch);
            bytes[1] = msg->data.cc.cc & 0x7Fu;
            bytes[2] = msg->data.cc.value & 0x7Fu;
            len = 3;
            break;

        case MIDI_MSG_PITCH_BEND: {
            uint16_t v = msg->data.pitchbend.value;
            if (v > 16383) v = 16383;
            bytes[0] = (uint8_t)(0xE0u | ch);
            bytes[1] = (uint8_t)(v & 0x7Fu);          /* LSB */
            bytes[2] = (uint8_t)((v >> 7) & 0x7Fu);   /* MSB */
            len = 3;
            break;
        }

        case MIDI_MSG_CH_PRESSURE:
            bytes[0] = (uint8_t)(0xD0u | ch);
            bytes[1] = msg->data.ch_pressure.value & 0x7Fu;
            len = 2;
            break;

        case MIDI_MSG_PROGRAM_CHANGE:
            bytes[0] = (uint8_t)(0xC0u | ch);
            bytes[1] = msg->data.program.program & 0x7Fu;
            len = 2;
            break;

        default:
            return false;
    }

    return send_bytes_to_routes(s_routes, bytes, len);
}

void midi_out_init_ex(const midi_out_config_t *cfg)
{
    if (s_inited) return;

    esp_log_level_set(TAG, ESP_LOG_INFO);

    if (cfg && cfg->routes != 0) {
        s_routes = cfg->routes;
    } else {
        s_routes = MIDI_OUT_ROUTE_USB;
#if CONFIG_EMIUET_MIDI_TRS_UART_ENABLE
        s_routes |= MIDI_OUT_ROUTE_TRS_UART;
#endif
    }

    /* Init backends. Safe to call even if route is off; backends may no-op. */
    (void)midi_out_usb_init();
    (void)midi_out_uart_trs_init();
    (void)midi_out_ble_init();

    s_inited = true;
    ESP_LOGI(TAG, "midi_out init routes=0x%08lx", (unsigned long)s_routes);
}

void midi_out_init(void)
{
    midi_out_init_ex(NULL);
}

void midi_out_set_routes(uint32_t routes)
{
    if (routes == 0) {
        /* never allow 'no route' silently; keep last setting */
        ESP_LOGW(TAG, "midi_out_set_routes(routes=0) ignored");
        return;
    }
    s_routes = routes;
    ESP_LOGI(TAG, "midi_out routes=0x%08lx", (unsigned long)s_routes);
}

uint32_t midi_out_get_routes(void)
{
    return s_routes;
}

bool midi_out_send(const midi_msg_t *msg)
{
    if (!s_inited) {
        midi_out_init_ex(NULL);
    }

    bool ok = encode_and_send(msg);
    if (!ok) {
        /* keep logs light; detailed backend errors are logged there */
        ESP_LOGD(TAG, "midi_out_send failed type=%d ch=%d", (int)msg->type, (int)msg->channel);
    }
    return ok;
}

/* =========================================================
 * Compatibility wrappers
 * ========================================================= */

void midi_send_note_on(uint8_t channel, uint8_t note, uint8_t velocity)
{
    midi_msg_t m = {
        .type = MIDI_MSG_NOTE_ON,
        .channel = channel,
    };
    m.data.note.note = note;
    m.data.note.velocity = velocity;
    (void)midi_out_send(&m);
}

void midi_send_note_off(uint8_t channel, uint8_t note, uint8_t velocity)
{
    midi_msg_t m = {
        .type = MIDI_MSG_NOTE_OFF,
        .channel = channel,
    };
    m.data.note.note = note;
    m.data.note.velocity = velocity;
    (void)midi_out_send(&m);
}

void midi_send_cc(uint8_t channel, uint8_t cc, uint8_t value)
{
    midi_msg_t m = {
        .type = MIDI_MSG_CC,
        .channel = channel,
    };
    m.data.cc.cc = cc;
    m.data.cc.value = value;
    (void)midi_out_send(&m);
}

void midi_send_pitchbend(uint8_t channel, uint16_t value)
{
    midi_msg_t m = {
        .type = MIDI_MSG_PITCH_BEND,
        .channel = channel,
    };
    m.data.pitchbend.value = value;
    (void)midi_out_send(&m);
}

void midi_send_ch_pressure(uint8_t channel, uint8_t value)
{
    midi_msg_t m = {
        .type = MIDI_MSG_CH_PRESSURE,
        .channel = channel,
    };
    m.data.ch_pressure.value = value;
    (void)midi_out_send(&m);
}

void midi_send_program_change(uint8_t channel, uint8_t program)
{
    midi_msg_t m = {
        .type = MIDI_MSG_PROGRAM_CHANGE,
        .channel = channel,
    };
    m.data.program.program = program;
    (void)midi_out_send(&m);
}
