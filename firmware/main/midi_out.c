#include "midi_out.h"
#include "esp_log.h"

static const char *TAG = "midi_out";

void midi_out_init(void)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);
}

void midi_send_note_on(uint8_t channel, uint8_t note, uint8_t velocity)
{
    ESP_LOGI(TAG, "NOTE_ON ch=%d note=%d vel=%d", channel, note, velocity);
}

void midi_send_note_off(uint8_t channel, uint8_t note, uint8_t velocity)
{
    ESP_LOGI(TAG, "NOTE_OFF ch=%d note=%d vel=%d", channel, note, velocity);
}

void midi_send_pitchbend(uint8_t channel, uint16_t value)
{
    ESP_LOGI(TAG, "PITCHBEND ch=%d value=%d", channel, value);
}
