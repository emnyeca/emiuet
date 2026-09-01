#include "midi_input.h"

#include <stdbool.h>
#include <string.h>
#include "led_control.h"

void midi_input_parser_init(midi_input_parser_t *parser)
{
    memset(parser, 0, sizeof(*parser));
}

static uint8_t message_data_length(uint8_t status)
{
    const uint8_t type = status & 0xF0u;
    return (type == 0xC0u || type == 0xD0u) ? 1u : 2u;
}

void midi_input_feed(midi_input_parser_t *parser, const uint8_t *bytes, size_t len)
{
    if (!parser || !bytes) return;
    for (size_t i = 0; i < len; ++i) {
        const uint8_t byte = bytes[i];
        if (byte >= 0xF8u) continue; /* real-time bytes do not disturb parsing */
        if (byte & 0x80u) {
            if (byte == 0xF0u) {
                parser->in_sysex = true;
                parser->running_status = 0;
            } else if (byte == 0xF7u) {
                parser->in_sysex = false;
            } else if (byte < 0xF0u) {
                parser->in_sysex = false;
                parser->running_status = byte;
                parser->needed = message_data_length(byte);
                parser->count = 0;
            } else {
                parser->running_status = 0;
                parser->count = 0;
            }
            continue;
        }
        if (parser->in_sysex || !parser->running_status) continue;
        parser->data[parser->count++] = byte & 0x7Fu;
        if (parser->count == parser->needed) {
            led_control_handle_midi(parser->running_status, parser->data[0],
                                    parser->needed == 2 ? parser->data[1] : 0);
            parser->count = 0;
        }
    }
}
