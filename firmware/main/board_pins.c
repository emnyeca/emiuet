#include "board_pins.h"
#include "driver/gpio.h"
#include "esp_err.h"

/* Row drive pins (Strings) */
const gpio_num_t MATRIX_ROW_PINS[MATRIX_NUM_ROWS] = {
    GPIO_NUM_5,   /* Str1 */
    GPIO_NUM_7,   /* Str2 */
    GPIO_NUM_8,   /* Str3 */
    GPIO_NUM_9,   /* Str4 */
    GPIO_NUM_11,  /* Str5 */
    GPIO_NUM_10   /* Str6 */
};

/* Column sense pins (Frets) */
const gpio_num_t MATRIX_COL_PINS[MATRIX_NUM_COLS] = {
    GPIO_NUM_46,  /* Frt0  (Strapping) */
    GPIO_NUM_45,  /* Frt1  (Strapping) */
    GPIO_NUM_35,  /* Frt2 */
    GPIO_NUM_36,  /* Frt3 */
    GPIO_NUM_37,  /* Frt4 */
    GPIO_NUM_34,  /* Frt5 */
    GPIO_NUM_33,  /* Frt6 */
    GPIO_NUM_47,  /* Frt7 */
    GPIO_NUM_21,  /* Frt8 */
    GPIO_NUM_15,  /* Frt9 */
    GPIO_NUM_14,  /* Frt10 */
    GPIO_NUM_13,  /* Frt11 */
    GPIO_NUM_12   /* Frt12 */
};

static void configure_input_with_pullup(gpio_num_t pin)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    (void)gpio_config(&io);
}

static void configure_input_no_pull(gpio_num_t pin)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    (void)gpio_config(&io);
}

static void configure_output(gpio_num_t pin, int initial_level)
{
    gpio_config_t io = {
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
    /* Status LED */
    configure_output(PIN_STATUS_LED, 0);

    /* Buttons
     * Assumption: buttons are active-low to GND. If your HW is different, adjust pulls here.
     */
    configure_input_with_pullup(PIN_SW_CENTER);
    configure_input_with_pullup(PIN_SW_RIGHT);
    configure_input_with_pullup(PIN_SW_LEFT);

    /* Power status pins have external pull-ups per pinout doc */
    configure_input_no_pull(PIN_CHG_STATUS);
    configure_input_no_pull(PIN_PGOOD_STATUS);

    /* NOTE:
     * Sliders and ADC pins are configured in ADC driver code, not here.
     * I2C pins are configured by the I2C driver / u8g2 layer.
     */
}
void board_pins_init_matrix_prepare(void)
{
    /* Configure matrix rows as outputs only. Keep columns untouched for now
     * to avoid changing strapping pin state until we are ready.
     */
    for (int r = 0; r < MATRIX_NUM_ROWS; ++r) {
        configure_output(MATRIX_ROW_PINS[r], 1);
    }
}

void board_pins_enable_matrix_columns(void)
{
    /* Now enable column inputs. By default we enable internal pull-ups for
     * prototype safety; override MATRIX_COL_INTERNAL_PULLUP to 0 if hardware
     * provides external resistors.
     */
    for (int c = 0; c < MATRIX_NUM_COLS; ++c) {
#if MATRIX_COL_INTERNAL_PULLUP
        configure_input_with_pullup(MATRIX_COL_PINS[c]);
#else
        configure_input_no_pull(MATRIX_COL_PINS[c]);
#endif
    }
}

void board_pins_init_matrix_late(void)
{
    board_pins_init_matrix_prepare();
    board_pins_enable_matrix_columns();
}
