#include "input_router.h"

void matrix_midi_bridge_start(int discard_cycles)
{
    input_router_start(discard_cycles);
}

void matrix_midi_bridge_stop(void)
{
    input_router_stop();
}
