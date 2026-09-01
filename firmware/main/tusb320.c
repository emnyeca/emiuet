#include "tusb320.h"

#include "board_i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "usb_power.h"

#define TUSB320_ADDR              0x60
#define TUSB320_REG_CURRENT       0x08
#define TUSB320_REG_ATTACH        0x09
#define TUSB320_REG_CONTROL       0x0A
#define TUSB320_SOFT_RESET        0x08

static const char *TAG = "tusb320";
static i2c_master_dev_handle_t s_dev;
static TaskHandle_t s_task;

static esp_err_t reg_read(uint8_t reg, uint8_t *value)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, value, 1, 100);
}

static esp_err_t reg_write(uint8_t reg, uint8_t value)
{
    const uint8_t data[2] = {reg, value};
    return i2c_master_transmit(s_dev, data, sizeof(data), 100);
}

static usb_current_mode_t decode_current(uint8_t reg08)
{
    switch ((reg08 >> 4) & 0x03u) {
        case 0x01: return USB_CURRENT_1P5A;
        case 0x03: return USB_CURRENT_3A;
        case 0x00: return USB_CURRENT_DEFAULT;
        default: return USB_CURRENT_UNKNOWN;
    }
}

static void poll_task(void *arg)
{
    (void)arg;
    unsigned polls = 0;
    usb_power_status_t last = usb_power_get_status();

    for (;;) {
        uint8_t current = 0;
        uint8_t attach = 0;
        if (reg_read(TUSB320_REG_CURRENT, &current) == ESP_OK &&
            reg_read(TUSB320_REG_ATTACH, &attach) == ESP_OK) {
            const bool sink = ((attach >> 6) & 0x03u) == 0x02u;
            const bool cc2 = (attach & 0x20u) != 0;
            const usb_current_mode_t mode = decode_current(current);
            usb_power_set_status(sink, cc2, mode);
            const usb_power_status_t now = usb_power_get_status();
            if (now.attached_as_sink != last.attached_as_sink ||
                now.orientation_cc2 != last.orientation_cc2 ||
                now.advertised_current != last.advertised_current) {
                ESP_LOGI(TAG, "sink=%d orientation=%s current=%d rgb_budget=%u mA",
                         now.attached_as_sink, now.orientation_cc2 ? "CC2" : "CC1",
                         now.advertised_current, now.rgb_budget_ma);
                last = now;
            }
        }

        /* TI requires periodic I2C soft reset to refresh CURRENT_MODE_DETECT
         * after a source changes its Rp while still attached.
         */
        if (++polls >= 20) {
            uint8_t control = 0;
            if (reg_read(TUSB320_REG_CONTROL, &control) == ESP_OK) {
                (void)reg_write(TUSB320_REG_CONTROL, control | TUSB320_SOFT_RESET);
            }
            polls = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

bool tusb320_start(void)
{
    if (s_task) return true;
    if (board_i2c_init() != ESP_OK) return false;
    const i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = TUSB320_ADDR,
        .scl_speed_hz = 400000,
    };
    if (i2c_master_bus_add_device(board_i2c_bus(), &cfg, &s_dev) != ESP_OK) {
        ESP_LOGE(TAG, "TUSB320 not found at 0x60");
        return false;
    }
    return xTaskCreatePinnedToCore(poll_task, "tusb320", 3072, NULL, 4,
                                   &s_task, 0) == pdPASS;
}
