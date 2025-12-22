#include "midi_out.h"

#include <stddef.h>

#include "esp_log.h"

static const char *TAG = "midi_out_uart_trs";

/*
 * TRS MIDI (Type-A) backend placeholder.
 *
 * NOTE: The design maps TRS MIDI OUT to PIN_MIDI_OUT_TX (UART0 TX). ESP-IDF default
 * console also uses UART0, so enabling this backend likely requires moving
 * console output away from UART0 (e.g., USB Serial JTAG) to avoid conflicts.
 */

bool midi_out_uart_trs_init(void)
{
    /* Not implemented yet; keep architecture stable. */
    ESP_LOGI(TAG, "TRS UART backend not enabled (stub)");
    return false;
}

bool midi_out_uart_trs_send_bytes(const uint8_t *bytes, size_t len)
{
    (void)bytes;
    (void)len;
    return false;
}
