#include "ui_oled.h"

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
#include "driver/gpio.h"

#include "adc_manager.h"
#include "board_pins.h"
#include "slider.h"

#include "matrix_scan.h"

#include "esp_timer.h"

#include "ui_led_status.h"

#include "controls.h"
#include "input_router.h"
#include "keyboard_input.h"
#include "usb_hid_keyboard.h"

#include "midi_mpe.h"
#include "midi_out.h"

#include "esp_log.h"


// -------------------------
// Pin / I2C config
// -------------------------
#define I2C_SCL_GPIO   ((int)PIN_I2C_SCL)
#define I2C_SDA_GPIO   ((int)PIN_I2C_SDA)
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
// Power UI / Debug inputs
// -------------------------
#define PIN_PGOOD           ((int)PIN_PGOOD_STATUS)
#define PIN_CHG             ((int)PIN_CHG_STATUS)

#define POWER_UPDATE_MS     500

// Battery icon geometry (yellow area)
#define BAT_X       2
#define BAT_Y       3
#define BAT_W       22
#define BAT_H       10
#define NUB_W       2
#define NUB_H       6

#define BAR_H       6
#define BAR_W       5
#define BAR_GAP     1

// Status bar icons (yellow area)
#define STATUS_ICON_Y              1
#define STATUS_ICON_MARGIN_RIGHT   2
#define STATUS_ICON_SPACING        2

#define STRINGWISE_ICON_W          24
#define BLE_ICON_W                 14

typedef enum {
    BLE_UI_DISABLED = 0,
    BLE_UI_ENABLED,
    BLE_UI_ADVERTISING,
} ble_ui_state_t;

// 初期閾値（未確定のままでOK：後で微調整）
#define V_TH_3_TO_2_MV   3950
#define V_TH_2_TO_1_MV   3750
#define V_TH_LOW_MV      3550

typedef enum {
    PWR_STATE_FAULT = 0,
    PWR_STATE_CHARGING,   // ⚡
    PWR_STATE_CHARGED,    // 3 bars fixed
    PWR_STATE_BAT_3,
    PWR_STATE_BAT_2,
    PWR_STATE_BAT_1,
    PWR_STATE_BAT_1_BLINK,
} power_ui_state_t;

typedef struct {
    power_ui_state_t state;
    int bars;              // 0..3（表示用）
    bool blink_on;         // 点滅のON/OFF（描画で使用）
} power_ui_t;

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
// ADC access (centralized)
// -------------------------
static bool s_adc_ok = false;

static void gpio_init_inputs(void)
{
    gpio_config_t io = {0};

    // PGOOD / CHG (pull-up already external, but internal pull-up harmless)
    io.mode = GPIO_MODE_INPUT;
    io.pin_bit_mask = (1ULL << PIN_PGOOD) | (1ULL << PIN_CHG);
    io.pull_up_en = 1;
    io.pull_down_en = 0;
    gpio_config(&io);
}

static void adc_init(void)
{
    s_adc_ok = adc_manager_init();
    if (!s_adc_ok) {
        ESP_LOGW("OLED", "adc_manager_init failed; falling back to slider proxy");
    }
}

static int read_adc_mv_batvsense(void)
{
    if (!s_adc_ok) {
        /* No ADC unit available; approximate battery from pitch-bend slider.
         * (This is a fallback path; normal behavior reads PIN_BAT_VSENSE via adc_manager.)
         */
        uint16_t raw = slider_read_pitchbend();
        int slider_mv = (raw * 3300) / 1023;
        return 3300 + (slider_mv * 900) / 3300; // same formula as before
    }

    int mv = 0;
    if (adc_manager_read_mv(PIN_BAT_VSENSE, &mv) != ESP_OK) {
        uint16_t raw = slider_read_pitchbend();
        int slider_mv = (raw * 3300) / 1023;
        return 3300 + (slider_mv * 900) / 3300;
    }
    return mv; // Vadc
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
    ESP_LOGI("BOOT", "boot anim start");
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
    ESP_LOGI("BOOT", "boot anim end");
}

static void draw_lightning(u8g2_t *u8g2, int x, int y)
{
    // 7x7 くらいの簡易⚡（線だけで描く）
    // 中央に収まるように置く想定
    u8g2_DrawLine(u8g2, x+4, y+0, x+1, y+4);
    u8g2_DrawLine(u8g2, x+1, y+4, x+4, y+4);
    u8g2_DrawLine(u8g2, x+4, y+4, x+2, y+7);
    u8g2_DrawLine(u8g2, x+2, y+7, x+6, y+3);
    u8g2_DrawLine(u8g2, x+6, y+3, x+4, y+3);
}

static void draw_battery_icon(u8g2_t *u8g2, const power_ui_t *p)
{
    // 点滅OFF時に「消える」対象があるので、消したい時は描かない。
    // ただし枠点滅(Fault)とバー点滅(low)を分ける。

    const bool fault = (p->state == PWR_STATE_FAULT);
    const bool lowblink = (p->state == PWR_STATE_BAT_1_BLINK);
    const bool charging = (p->state == PWR_STATE_CHARGING);

    // 枠（Fault時は枠を点滅）
    if (!fault || p->blink_on) {
        u8g2_DrawFrame(u8g2, BAT_X, BAT_Y, BAT_W, BAT_H);
        // nub
        int nub_y = BAT_Y + (BAT_H - NUB_H) / 2;
        u8g2_DrawBox(u8g2, BAT_X + BAT_W, nub_y, NUB_W, NUB_H);
    }

    if (fault) {
        // 0 bars、枠点滅のみ
        return;
    }

    if (charging) {
        // ⚡のみ（アニメ無し）
        int cx = BAT_X + (BAT_W - 7) / 2;
        int cy = BAT_Y + (BAT_H - 7) / 2;
        draw_lightning(u8g2, cx, cy);
        return;
    }

    // バー描画（chargedは3固定、batteryはbarsに従う）
    int bars = p->bars; // 0..3
    if (bars < 0) bars = 0;
    if (bars > 3) bars = 3;

    // 内部の左上（バー基準位置）
    int inner_x = BAT_X + 2;
    int inner_y = BAT_Y + 2;

    // 低電圧警告：1バーのみ、バーだけ点滅
    bool draw_bars = true;
    if (lowblink && !p->blink_on) {
        draw_bars = false;
    }

    if (!draw_bars) return;

    for (int i = 0; i < bars; i++) {
        int bx = inner_x + i * (BAR_W + BAR_GAP);
        u8g2_DrawBox(u8g2, bx, inner_y, BAR_W, BAR_H);
    }
}

static power_ui_t s_pwr_ui = {0};

static int calc_bars_from_vbat(int vbat_mv)
{
    if (vbat_mv >= V_TH_3_TO_2_MV) return 3;
    if (vbat_mv >= V_TH_2_TO_1_MV) return 2;
    if (vbat_mv >= V_TH_LOW_MV)    return 1;
    return 1; // low warningでもバーは1（点滅で表現）
}

static void power_ui_update_500ms(power_ui_t *p)
{
    // 実ピン読み（デバッグ時は上書き）
    bool ext_power = (gpio_get_level(PIN_PGOOD) == 0);
    bool charging  = (gpio_get_level(PIN_CHG) == 0);

    if (ext_power) {
        if (charging) {
            p->state = PWR_STATE_CHARGING; // ⚡
            p->bars = 0;
        } else {
            p->state = PWR_STATE_CHARGED;  // 3バー固定
            p->bars = 3;
        }
        return;
    }

    // ---- Battery mode ----
    /* Read vbat from PIN_BAT_VSENSE. */
    int vbat_mv = read_adc_mv_batvsense(); // read battery sense (mV)

    int bars = calc_bars_from_vbat(vbat_mv);
    p->bars = bars;

    if (vbat_mv < V_TH_LOW_MV) {
        p->state = PWR_STATE_BAT_1_BLINK;
    } else if (bars == 1) {
        p->state = PWR_STATE_BAT_1;
    } else if (bars == 2) {
        p->state = PWR_STATE_BAT_2;
    } else {
        p->state = PWR_STATE_BAT_3;
    }
}

static led_state_t led_state_from_power_ui(const power_ui_t *p)
{
    switch (p->state) {
        case PWR_STATE_FAULT:
            return LED_ST_FAULT;

        case PWR_STATE_CHARGING:
            return LED_ST_CHARGING;

        case PWR_STATE_CHARGED:
            return LED_ST_CHARGED;

        case PWR_STATE_BAT_1_BLINK:
            return LED_ST_LOW_BATT;

        case PWR_STATE_BAT_1:
        case PWR_STATE_BAT_2:
        case PWR_STATE_BAT_3:
        default:
            return LED_ST_SYSTEM_NORMAL;
    }
}

static bool power_ui_is_fault(const power_ui_t *p) { return p->state == PWR_STATE_FAULT; }
static bool power_ui_is_lowblink(const power_ui_t *p) { return p->state == PWR_STATE_BAT_1_BLINK; }

static bool stringwise_is_enabled(void)
{
    /* Emiuet naming: "Stringwise Bend" == MPE-style per-string channel mode. */
    return midi_mpe_is_enabled();
}

/* Optional hook for future BLE implementation.
 * Return values must match ble_ui_state_t.
 */
__attribute__((weak)) int emiuet_ble_ui_get_state(void)
{
    return (int)BLE_UI_ENABLED;
}

static ble_ui_state_t ble_get_state(void)
{
    /* Treat BLE as disabled unless the BLE route is enabled and BT is enabled in sdkconfig.
     * This keeps the icon off by default (current BLE backend is a stub).
     */
#if defined(CONFIG_BT_ENABLED) && (CONFIG_BT_ENABLED)
    if ((midi_out_get_routes() & MIDI_OUT_ROUTE_BLE) == 0) return BLE_UI_DISABLED;

    int st = emiuet_ble_ui_get_state();
    if (st < (int)BLE_UI_DISABLED) st = (int)BLE_UI_DISABLED;
    if (st > (int)BLE_UI_ADVERTISING) st = (int)BLE_UI_ADVERTISING;
    return (ble_ui_state_t)st;
#else
    return BLE_UI_DISABLED;
#endif
}

static bool ui_blink_on_2hz(int64_t now_ms)
{
    /* 2Hz: 250ms ON/OFF */
    return ((now_ms / 250) % 2) == 0;
}

static bool ui_needs_fast_refresh(const power_ui_t *p, ble_ui_state_t ble_state)
{
    if (power_ui_is_fault(p) || power_ui_is_lowblink(p)) return true;
    return ble_state == BLE_UI_ADVERTISING;
}

static void draw_stringwise_icon(u8g2_t *u8g2, int x, int y)
{
    /* Design: 6 horizontal strings + center ▶ + right-side vertical line.
     * Keep within 0..15 (yellow area).
     */
    const int x0 = x;
    const int x1 = x + 16;
    const int vx = x + 12;

    for (int i = 0; i < 4; i++) {
        int yy = y + 2 + (i * 3);
        u8g2_DrawHLine(u8g2, x0, yy, (x1 - x0));
    }

    /* ▶ (triangle outline) */
    const int cy = y + 7;
    u8g2_DrawTriangle(u8g2, x + 4, cy - 4, x + 4, cy + 4, x + 9, cy);

    /* Right-side vertical line ("fret/lock") */
    u8g2_DrawVLine(u8g2, vx, y + 2, 11);
}

static void draw_ble_icon(u8g2_t *u8g2, int x, int y)
{
    // Bluetooth rune glyph (approx) in 16x14 using lines only.
    // x,y: top-left of 16x16 box (icon uses y+1 .. y+14)
    // Geometry (14px tall)
    const int top = y + 2;      // was y+1
    const int bot = y + 13;     // was y+14
    const int mid = (top + bot) / 2;
    const int cx  = x + 7;      // stem center

    // Stem
    u8g2_DrawVLine(u8g2, cx, top, bot - top + 1);

    // Right-side triangles
    u8g2_DrawLine(u8g2, cx, top, x + 12, y + 5);
    u8g2_DrawLine(u8g2, cx, mid, x + 12, y + 5);

    u8g2_DrawLine(u8g2, cx, mid, x + 12, y + 10);
    u8g2_DrawLine(u8g2, cx, bot, x + 12, y + 10);

    // Left cross strokes
    u8g2_DrawLine(u8g2, cx, y + 6,  x + 3, y + 3);
    u8g2_DrawLine(u8g2, cx, y + 9,  x + 3, y + 12);
}

// 点滅位相：描画のたびにこれを更新して使う
static void power_ui_update_blink_phase(power_ui_t *p, int64_t now_ms)
{
    if (power_ui_is_fault(p)) {
        // Fault 2Hz: 250ms ON/OFF
        p->blink_on = ((now_ms / 250) % 2) == 0;
    } else if (power_ui_is_lowblink(p)) {
        // Low 1Hz: 500ms ON/OFF
        p->blink_on = ((now_ms / 500) % 2) == 0;
    } else {
        p->blink_on = true;
    }
}

static void draw_fixed_layout(u8g2_t *u8g2)
{
    // --- Yellow area (top): Battery + active input mode ---
    draw_battery_icon(u8g2, &s_pwr_ui);

    // ---- Cell size presets ----
    // Balanced: fits nicely with margins
    // (8,7,gap1) => grid_w=116, grid_h=47 in a 128x48 area
    const grid_layout_t g = grid_make_layout(8, 7, 1, 1);

    // If you want bigger blocks:
    // const grid_layout_t g = grid_make_layout(9, 7, 0, 1); // grid_w=117, grid_h=47
    // Compact:
    // const grid_layout_t g = grid_make_layout(8, 6, 1, 1); // grid_h=41

    u8g2_SetFont(u8g2, u8g2_font_5x7_tf);
    u8g2_SetFontPosBaseline(u8g2);

    char mode_text[20];
    if (input_router_get_mode() == INPUT_MODE_KEYBOARD) {
        (void)snprintf(mode_text, sizeof(mode_text), "TYPE L%u C:%s",
                       (unsigned)keyboard_input_get_layer(),
                       usb_hid_keyboard_caps_lock_on() ? "ON" : "OFF");
    } else {
        (void)snprintf(mode_text, sizeof(mode_text), "MIDI OCT:%d", (int)controls_get_octave());
    }
    int tw = u8g2_GetStrWidth(u8g2, mode_text);
    int tx = (OLED_W - tw) / 2;
    int ty = 11; // baseline within 0..15
    u8g2_DrawStr(u8g2, tx, ty, mode_text);

    /* --- Yellow area: status icons (right side) --- */
    const int stringwise_x = OLED_W - STATUS_ICON_MARGIN_RIGHT - STRINGWISE_ICON_W;
    const int ble_x = stringwise_x - STATUS_ICON_SPACING - BLE_ICON_W;

    if (stringwise_is_enabled()) {
        draw_stringwise_icon(u8g2, stringwise_x, STATUS_ICON_Y);
    }

    ble_ui_state_t ble_state = ble_get_state();
    if (ble_state != BLE_UI_DISABLED) {
        bool blink_on = true;
        if (ble_state == BLE_UI_ADVERTISING) {
            int64_t now_ms = esp_timer_get_time() / 1000;
            blink_on = ui_blink_on_2hz(now_ms);
        }
        if (blink_on) {
            draw_ble_icon(u8g2, ble_x, STATUS_ICON_Y);
        }
    }

    // boundary line at y=15 (optional, but nice)
    u8g2_DrawHLine(u8g2, 0, YELLOW_H - 1, OLED_W);

        // --- Blue area: 6x13 ---
        // Draw current matrix pressed state (real-time) instead of fixed demo chords.
        for (int r = 0; r < GRID_ROWS; r++) {
            for (int c = 0; c < GRID_COLS; c++) {
                int x = col_to_x(&g, c);
                int y = g.origin_y + r * (g.cell_h + g.gap_y);

                bool on = matrix_scan_is_pressed(r, c);

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

    // ここから先でステータス表示を開始
    gpio_init_inputs();
    adc_init();

    int64_t next_update_ms = 0;
    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        int64_t now_ms = esp_timer_get_time() / 1000;

        // 500ms周期で入力更新
        if (now_ms >= next_update_ms) {
            next_update_ms = now_ms + POWER_UPDATE_MS;
            power_ui_update_500ms(&s_pwr_ui);

            led_status_set_state(led_state_from_power_ui(&s_pwr_ui));
        }


        // 点滅位相更新（ループ頻度が高いほど滑らか）
        power_ui_update_blink_phase(&s_pwr_ui, now_ms);

        // 描画（点滅を見せるため定期リフレッシュ）
        u8g2_FirstPage(&s_u8g2);
        do {
            draw_fixed_layout(&s_u8g2);
        } while (u8g2_NextPage(&s_u8g2));

        /* Adaptive refresh rate:
         * - Normal: 100ms (10fps)
         * - When blinking (power fault/low-batt or BLE advertising): 50ms (20fps)
         */
        ble_ui_state_t ble_state = ble_get_state();
        bool fast = ui_needs_fast_refresh(&s_pwr_ui, ble_state);
        int frame_ms = fast ? 50 : 100;

        TickType_t frame_ticks = pdMS_TO_TICKS(frame_ms);
        if (frame_ticks < 1) frame_ticks = 1;
        vTaskDelayUntil(&last_wake, frame_ticks);
    }
}

void oled_demo_start(void)
{
    xTaskCreatePinnedToCore(oled_task, "OledDemo", 4096, NULL, 3, NULL, 0);
}
