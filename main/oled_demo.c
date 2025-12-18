#include "oled_demo.h"

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c_master.h"   // ESP-IDF v5.x new I2C driver
#include "esp_err.h"

// u8g2
#include "u8g2.h"

#include "esp_rom_sys.h"   // esp_rom_delay_us
#include "emiuet_logo.xbm"


// -------------------------
// Pin / I2C config
// -------------------------
#define I2C_SCL_GPIO   16
#define I2C_SDA_GPIO   18
#define I2C_CLK_HZ     400000   // 400kHz (safe for most OLED modules)

// -------------------------
// Globals
// -------------------------
static i2c_master_bus_handle_t s_i2c_bus = NULL;
static i2c_master_dev_handle_t s_i2c_dev = NULL;
static u8g2_t s_u8g2;

// -------------------------
// I2C init + scan
// -------------------------
static void i2c_init_and_scan(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .scl_io_num = I2C_SCL_GPIO,
        .sda_io_num = I2C_SDA_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = false,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &s_i2c_bus));

    printf("[OLED] I2C init OK (SCL=GPIO%d, SDA=GPIO%d, %d Hz)\n",
           I2C_SCL_GPIO, I2C_SDA_GPIO, I2C_CLK_HZ);

    // Scan typical range
    int found = 0;
    for (int addr = 0x03; addr <= 0x77; addr++) {
        esp_err_t err = i2c_master_probe(s_i2c_bus, addr, 50 /*ms*/);
        if (err == ESP_OK) {
            printf("[OLED] Found I2C device at 0x%02X\n", addr);
            found++;
        }
    }

    if (!found) {
        printf("[OLED] No I2C devices found. Check wiring/power/pins.\n");
    }
}

// -------------------------
// Layout constants
// -------------------------
#define OLED_W      128
#define OLED_H      64

// 2-color OLED: top area is physically yellow (common: 16px)
#define YELLOW_H    16

// Grid: 6 strings x 13 frets
#define GRID_ROWS   6
#define GRID_COLS   13

#define OPEN_GAP_EXTRA  2   // 0フレットと1フレットの間を広げる(px)

typedef struct {
    int cell_w, cell_h;
    int gap_x, gap_y;
    int origin_x, origin_y;
    int grid_w, grid_h;
} grid_layout_t;

static grid_layout_t grid_make_layout(int cell_w, int cell_h, int gap_x, int gap_y)
{
    grid_layout_t g = {0};
    g.cell_w = cell_w;
    g.cell_h = cell_h;
    g.gap_x  = gap_x;
    g.gap_y  = gap_y;

    const int area_x = 0;
    const int area_y = YELLOW_H;
    const int area_w = OLED_W;
    const int area_h = OLED_H - YELLOW_H;

    g.grid_w = GRID_COLS * cell_w + (GRID_COLS - 1) * gap_x + OPEN_GAP_EXTRA;
    g.grid_h = GRID_ROWS * cell_h + (GRID_ROWS - 1) * gap_y;

    g.origin_x = area_x + (area_w - g.grid_w) / 2;
    g.origin_y = area_y + (area_h - g.grid_h) / 2;

    // Clamp
    if (g.origin_x < 0) g.origin_x = 0;
    if (g.origin_y < area_y) g.origin_y = area_y;

    return g;
}

// -------------------------
// Grid UI tweaks
// -------------------------

static inline bool is_marker_fret(int c)
{
    return (c == 3 || c == 5 || c == 7 || c == 9 || c == 12);
}

static inline int col_to_x(const grid_layout_t *g, int c)
{
    int x = g->origin_x + c * (g->cell_w + g->gap_x);
    if (c >= 1) x += OPEN_GAP_EXTRA;   // 列1以降を右にずらす
    return x;
}

static void draw_cell_doublebox_fill(
    u8g2_t *u8g2,
    int x, int y, int w, int h,
    bool on,
    bool marker,
    bool draw_marker_line)
{
    // 外枠
    u8g2_DrawFrame(u8g2, x, y, w, h);

    // 押下：内側を塗りつぶし（枠2本分を残す）
    if (on) {
        // 2pxインセットして塗る（外枠+内枠が残る）
        if (w >= 7 && h >= 7) {
            u8g2_DrawBox(u8g2, x + 2, y + 2, w - 4, h - 4);
        } else if (w > 2 && h > 2) {
            // 小さすぎる時のフォールバック
            u8g2_DrawBox(u8g2, x + 1, y + 1, w - 2, h - 2);
        } else {
            u8g2_DrawBox(u8g2, x, y, w, h);
        }
    }

    // マーカーフレット下線（最下段は描かない）
    if (marker && draw_marker_line) {
        int yy = y + h;
        if (yy < OLED_H) {
            u8g2_DrawHLine(u8g2, x, yy, w);
        }
    }
}

// -------------------------
// u8g2 callbacks (ESP-IDF v5 I2C master)
// -------------------------

// u8g2 uses 8-bit address (7-bit << 1). We'll convert when talking to ESP-IDF.
static uint8_t u8x8_byte_esp32_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    static uint8_t buffer[256];
    static uint16_t buf_idx = 0;

    switch (msg) {
    case U8X8_MSG_BYTE_INIT:
        // nothing (I2C already init)
        return 1;

    case U8X8_MSG_BYTE_START_TRANSFER:
        buf_idx = 0;
        return 1;

    case U8X8_MSG_BYTE_SEND: {
        uint8_t *data = (uint8_t *)arg_ptr;
        if (buf_idx + arg_int > sizeof(buffer)) return 0;
        memcpy(&buffer[buf_idx], data, arg_int);
        buf_idx += arg_int;
        return 1;
    }

    case U8X8_MSG_BYTE_END_TRANSFER: {
        if (s_i2c_dev == NULL) return 0;
        esp_err_t err = i2c_master_transmit(s_i2c_dev, buffer, buf_idx, 100);
        if (err != ESP_OK) {
            static int warned = 0;
            if (!warned) {
                warned = 1;
                printf("[OLED] i2c_master_transmit failed: %s\n", esp_err_to_name(err));
                printf("[OLED] Stopping OLED task to avoid log spam.\n");
            }
            vTaskDelete(NULL); // ← タスク終了でスパム止め
        }
        return 1;
    }

    default:
        return 0;
    }
}

static uint8_t u8x8_gpio_delay_esp32(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    (void)u8x8;
    (void)arg_ptr;

    switch (msg) {
    case U8X8_MSG_DELAY_MILLI:
        vTaskDelay(pdMS_TO_TICKS(arg_int));
        return 1;
    case U8X8_MSG_DELAY_10MICRO:
        // good enough for I2C OLED; yield a tiny bit
        esp_rom_delay_us(10 * arg_int);
        return 1;
    default:
        return 1;
    }
}

// -------------------------
// OLED demo task
// -------------------------

static void draw_logo_bitmap(u8g2_t *u8g2)
{
    const int logo_w = emiuet_logo_width;
    const int logo_h = emiuet_logo_height;

    const int x = (OLED_W - logo_w) / 2;
    const int y = YELLOW_H + (OLED_H - YELLOW_H - logo_h) / 2;

    u8g2_DrawXBMP(u8g2, x, y, logo_w, logo_h, emiuet_logo_bits);
}

static void draw_boot_tagline(u8g2_t *u8g2)
{
    // 黄色領域(0..15)に収める
    // 文字数が多いので細めフォント推奨
    u8g2_SetFont(u8g2, u8g2_font_5x7_tf);
    u8g2_SetFontPosBaseline(u8g2);

    const char *t = "Emnyeca's Utility Builds";
    int tw = u8g2_GetStrWidth(u8g2, t);

    // 左右どちらでも良いけど、中央寄せが“銘板感”
    int x = (OLED_W - tw) / 2;
    if (x < 0) x = 0; // もし入り切らない時の保険

    int y = 12; // baseline: 黄色領域内
    u8g2_DrawStr(u8g2, x, y, t);

    // 境界線（お好み）
    u8g2_DrawHLine(u8g2, 0, YELLOW_H - 1, OLED_W);
}

static void draw_boot_screen(u8g2_t *u8g2)
{
    draw_boot_tagline(u8g2);   // 黄エリア（0..15）
    draw_logo_bitmap(u8g2);    // 青エリア（16..63）
}

static void boot_logo_anim(u8g2_t *u8g2)
{
    const int frames = 22;
    const int delay_ms = 30;

    for (int i = 0; i <= frames; i++) {

        // 隠す高さ：最初は全画面、最後は0
        int cover_h = (OLED_H * (frames - i)) / frames;

        u8g2_FirstPage(u8g2);
        do {
            draw_boot_screen(u8g2);

            // 下からオープン = 上側を隠す（上が隠れてる間、下が見える）
            u8g2_SetDrawColor(u8g2, 0);
            u8g2_DrawBox(u8g2, 0, 0, OLED_W, cover_h);   // ★ y=0固定
            u8g2_SetDrawColor(u8g2, 1);

        } while (u8g2_NextPage(u8g2));

        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }

    vTaskDelay(pdMS_TO_TICKS(2000));
}

static void draw_fixed_layout(u8g2_t *u8g2)
{
    // ---- Cell size presets ----
    // Balanced: fits nicely with margins
    // (8,7,gap1) => grid_w=116, grid_h=47 in a 128x48 area
    const grid_layout_t g = grid_make_layout(8, 7, 1, 1);

    // If you want bigger blocks:
    // const grid_layout_t g = grid_make_layout(9, 7, 0, 1); // grid_w=117, grid_h=47
    // Compact:
    // const grid_layout_t g = grid_make_layout(8, 6, 1, 1); // grid_h=41

    // --- Yellow area (top): OCT: 0 ---
    u8g2_SetFont(u8g2, u8g2_font_6x12_tf);
    u8g2_SetFontPosBaseline(u8g2);

    const char *oct_text = "OCT: 0";
    int tw = u8g2_GetStrWidth(u8g2, oct_text);
    int tx = (OLED_W - tw) / 2;
    int ty = 12; // baseline within 0..15
    u8g2_DrawStr(u8g2, tx, ty, oct_text);

    // boundary line at y=15 (optional, but nice)
    u8g2_DrawHLine(u8g2, 0, YELLOW_H - 1, OLED_W);

        // Cmaj7 (open) = x32000
    // rows: 0=1弦(高E) ... 5=6弦(低E)
    static const int8_t fingering_cmaj7[6] = {
         3, // 1弦 ミュート
         5, // 2弦
         4, // 3弦
         5, // 4弦
         3, // 5弦
        -1  // 6弦
    };

    // --- Blue area: 6x13 ---
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            int x = col_to_x(&g, c);
            int y = g.origin_y + r * (g.cell_h + g.gap_y);

            int fret = fingering_cmaj7[r];
            bool on = (fret >= 0 && fret == c);

            bool marker = is_marker_fret(c);
            bool draw_marker_line = (r != GRID_ROWS - 1);

            draw_cell_doublebox_fill(
                u8g2,
                x, y,
                g.cell_w, g.cell_h,
                on,
                marker,
                draw_marker_line
            );
        }
    }

    // Optional: outer border for debugging layout
    // u8g2_DrawFrame(u8g2, 0, 0, OLED_W, OLED_H);
}

static void oled_task(void *arg)
{
    (void)arg;

    i2c_init_and_scan();

    // Most SSD1315 I2C modules respond at 0x3C (sometimes 0x3D).
    // We'll try 0x3C first; if it fails, you can switch to 0x3D.
    const uint8_t oled_addr_7bit = 0x3D;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = oled_addr_7bit,
        .scl_speed_hz = I2C_CLK_HZ,
    };

    esp_err_t err = i2c_master_bus_add_device(s_i2c_bus, &dev_cfg, &s_i2c_dev);
    if (err != ESP_OK) {
        printf("[OLED] Failed to add I2C device 0x%02X (%s)\n", oled_addr_7bit, esp_err_to_name(err));
        printf("[OLED] If your module is 0x3D, change oled_addr_7bit to 0x3D.\n");
        vTaskDelete(NULL);
        return;
    }

    printf("[OLED] Using I2C address 0x%02X\n", oled_addr_7bit);

    // u8g2 setup: SSD1306-compatible init works for many SSD1315 modules.
    // Use page buffer mode (_1) to keep RAM low.
    u8g2_Setup_ssd1315_i2c_128x64_noname_1(
        &s_u8g2,
        U8G2_R0,
        u8x8_byte_esp32_i2c,
        u8x8_gpio_delay_esp32
    );

    // u8g2 expects 8-bit address (7-bit << 1)
    u8g2_SetI2CAddress(&s_u8g2, (oled_addr_7bit << 1));

    u8g2_InitDisplay(&s_u8g2);
    u8g2_SetPowerSave(&s_u8g2, 0);

        // 起動ロゴアニメ
    u8g2_SetContrast(&s_u8g2, 64);   // 少し暗めで開始
    boot_logo_anim(&s_u8g2);
    u8g2_SetContrast(&s_u8g2, 255);

    printf("[OLED] u8g2 init done. Drawing...\n");

    // ---- draw once (fixed demo) ----
    u8g2_FirstPage(&s_u8g2);
    do {
        draw_fixed_layout(&s_u8g2);
    } while (u8g2_NextPage(&s_u8g2));

    printf("[OLED] Fixed layout drawn. Sleeping...\n");

    // Keep task alive (no redraw needed)
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void oled_demo_start(void)
{
    xTaskCreatePinnedToCore(oled_task, "OledDemo", 4096, NULL, 3, NULL, 0);
}
