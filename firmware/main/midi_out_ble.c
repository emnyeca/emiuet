#include "midi_out.h"

#include <stddef.h>

#include "esp_log.h"

static const char *TAG = "midi_out_ble";

/*
 * BLE-MIDI backend placeholder.
 * Implement later (requires BLE stack + state machine).
 */

bool midi_out_ble_init(void)
{
    ESP_LOGI(TAG, "BLE-MIDI backend not enabled (stub)");
    return false;
}

bool midi_out_ble_send_bytes(const uint8_t *bytes, size_t len)
{
    (void)bytes;
    (void)len;
    return false;
}
