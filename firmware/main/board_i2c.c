#include "board_i2c.h"

#include "board_pins.h"

static i2c_master_bus_handle_t s_bus;

esp_err_t board_i2c_init(void)
{
    if (s_bus) return ESP_OK;
    const i2c_master_bus_config_t cfg = {
        .i2c_port = I2C_NUM_0,
        .scl_io_num = PIN_I2C_SCL,
        .sda_io_num = PIN_I2C_SDA,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = false,
    };
    return i2c_new_master_bus(&cfg, &s_bus);
}

i2c_master_bus_handle_t board_i2c_bus(void)
{
    return s_bus;
}
