#include "board_pins.h"

#include "driver/gpio.h"

/* The two contiguous groups keep the 6x13 matrix easy to escape on the PCB. */
const gpio_num_t MATRIX_ROW_PINS[MATRIX_NUM_ROWS] = {
    GPIO_NUM_5, GPIO_NUM_6, GPIO_NUM_7,
    GPIO_NUM_8, GPIO_NUM_9, GPIO_NUM_10,
};

const gpio_num_t MATRIX_COL_PINS[MATRIX_NUM_COLS] = {
    GPIO_NUM_11, GPIO_NUM_12, GPIO_NUM_13, GPIO_NUM_14,
    GPIO_NUM_15, GPIO_NUM_16, GPIO_NUM_17, GPIO_NUM_18,
    GPIO_NUM_21,
    GPIO_NUM_33, GPIO_NUM_34, GPIO_NUM_35, GPIO_NUM_36,
};

static void configure_input(gpio_num_t pin, gpio_pullup_t pullup)
{
    const gpio_config_t io = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = pullup,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    (void)gpio_config(&io);
}

static void configure_output(gpio_num_t pin, int initial_level)
{
    const gpio_config_t io = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    (void)gpio_config(&io);
    (void)gpio_set_level(pin, initial_level);
}

void board_pins_init_early(void)
{
    configure_output(PIN_STATUS_LED, 0);
    configure_input(PIN_SW_CENTER, GPIO_PULLUP_ENABLE);
    configure_input(PIN_SW_RIGHT, GPIO_PULLUP_ENABLE);
    configure_input(PIN_SW_LEFT, GPIO_PULLUP_ENABLE);
    configure_input(PIN_TUSB320_INT_N, GPIO_PULLUP_DISABLE);
}

void board_pins_init_matrix_prepare(void)
{
    for (int row = 0; row < MATRIX_NUM_ROWS; ++row) {
        configure_input(MATRIX_ROW_PINS[row],
                        MATRIX_ROW_INTERNAL_PULLUP ? GPIO_PULLUP_ENABLE
                                                   : GPIO_PULLUP_DISABLE);
    }
}

void board_pins_enable_matrix_columns(void)
{
    for (int col = 0; col < MATRIX_NUM_COLS; ++col) {
        configure_output(MATRIX_COL_PINS[col], 1);
    }
}

void board_pins_init_matrix_late(void)
{
    board_pins_init_matrix_prepare();
    board_pins_enable_matrix_columns();
}
