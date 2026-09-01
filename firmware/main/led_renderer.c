#include "led_renderer.h"

#include "board_pins.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_control.h"
#include "led_strip.h"
#include "led_strip_rmt.h"
#include "sdkconfig.h"
#include "usb_power.h"

#ifndef CONFIG_EMIUET_RGB_ENABLE
#define CONFIG_EMIUET_RGB_ENABLE 0
#endif

#define CONSERVATIVE_FULL_WHITE_MA 60u

static const char *TAG = "led_renderer";
static led_strip_handle_t s_strip;
static TaskHandle_t s_task;

static void render_task(void *arg)
{
    (void)arg;
    led_rgb_t pixels[EMUIET_LED_COUNT];
    uint32_t rendered_generation = UINT32_MAX;
    uint16_t rendered_budget = UINT16_MAX;
    for (;;) {
        uint8_t brightness = 0;
        const uint32_t generation = led_control_snapshot(pixels, &brightness);
        const uint16_t budget = usb_power_get_status().rgb_budget_ma;
        if (generation != rendered_generation || budget != rendered_budget) {
            uint32_t channel_sum = 0;
            for (unsigned i = 0; i < EMUIET_LED_COUNT; ++i) {
                channel_sum += pixels[i].red + pixels[i].green + pixels[i].blue;
            }
            uint32_t estimated_ma = channel_sum * brightness * CONSERVATIVE_FULL_WHITE_MA;
            estimated_ma /= (255u * 255u * 3u);
            uint16_t budget_scale = 255;
            if (estimated_ma > budget && estimated_ma != 0) {
                budget_scale = (uint16_t)((uint32_t)budget * 255u / estimated_ma);
            }
            for (unsigned i = 0; i < EMUIET_LED_COUNT; ++i) {
                const uint32_t scale = (uint32_t)brightness * budget_scale;
                const uint8_t r = (uint8_t)((uint32_t)pixels[i].red * scale / (255u * 255u));
                const uint8_t g = (uint8_t)((uint32_t)pixels[i].green * scale / (255u * 255u));
                const uint8_t b = (uint8_t)((uint32_t)pixels[i].blue * scale / (255u * 255u));
                (void)led_strip_set_pixel(s_strip, i, r, g, b);
            }
            (void)led_strip_refresh(s_strip);
            rendered_generation = generation;
            rendered_budget = budget;
        }
        vTaskDelay(pdMS_TO_TICKS(16));
    }
}

bool led_renderer_start(void)
{
#if !CONFIG_EMIUET_RGB_ENABLE
    ESP_LOGI(TAG, "RGB disabled by configuration");
    return false;
#else
    if (s_task) return true;
    led_control_init();
    const led_strip_config_t strip_cfg = {
        .strip_gpio_num = PIN_RGB_DATA,
        .max_leds = EMUIET_LED_COUNT,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags.invert_out = false,
    };
    const led_strip_rmt_config_t rmt_cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10000000,
        .mem_block_symbols = 0,
        .flags.with_dma = true,
    };
    if (led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &s_strip) != ESP_OK) {
        ESP_LOGE(TAG, "failed to allocate RMT LED strip");
        return false;
    }
    (void)led_strip_clear(s_strip);
    return xTaskCreatePinnedToCore(render_task, "rgb_render", 4096, NULL, 4,
                                   &s_task, 1) == pdPASS;
#endif
}
