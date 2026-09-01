#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t running_status;
    uint8_t data[2];
    uint8_t count;
    uint8_t needed;
    bool in_sysex;
} midi_input_parser_t;

void midi_input_parser_init(midi_input_parser_t *parser);
void midi_input_feed(midi_input_parser_t *parser, const uint8_t *bytes, size_t len);
