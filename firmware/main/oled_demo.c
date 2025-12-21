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
#include "driver/gpio.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "slider.h"

#include "esp_timer.h"

#include "led_status.h"

#include "esp_log.h"


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
// Power UI / Debug inputs
// -------------------------
#define PIN_PGOOD           38
#define PIN_CHG             48

#define PIN_DBG_SLIDER      4    // ADC
#define PIN_DBG_BUTTON      40   // GPIO, pull-up, press=0

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

// 初期閾値（未確定のままでOK：後で微調整）
#define V_TH_3_TO_2_MV   3950
#define V_TH_2_TO_1_MV   3750
#define V_TH_LOW_MV      3550

typedef enum {
    PWR_MODE_BATTERY = 0,   // 外部電源なし
    PWR_MODE_EXT_CHARGING = 1,   // 外部電源ありで充電中
    PWR_MODE_EXT_CHARGED = 2,    // 外部電源ありで充電完了
    PWR_MODE_FAULT = 3,   // Fault表示
} power_debug_mode_t;

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
// ADC (oneshot) handles
// -------------------------
static adc_oneshot_unit_handle_t s_adc1 = NULL;
static adc_cali_handle_t s_adc_cali = NULL;
static bool s_adc_cali_ok = false;

// デバッグ用モード（GPIO40で巡回）
static power_debug_mode_t s_dbg_mode = PWR_MODE_BATTERY;

// ボタンデバウンス用
static int s_btn_last = 1;
static int64_t s_btn_last_change_us = 0;

static void gpio_init_inputs(void)
{
    gpio_config_t io = {0};

    // PGOOD / CHG (pull-up already external, but internal pull-up harmless)
    io.mode = GPIO_MODE_INPUT;
    io.pin_bit_mask = (1ULL << PIN_PGOOD) | (1ULL << PIN_CHG);
    io.pull_up_en = 1;
    io.pull_down_en = 0;
    gpio_config(&io);

    // Debug button (GPIO40)
    io.pin_bit_mask = (1ULL << PIN_DBG_BUTTON);
    io.pull_up_en = 1;
    io.pull_down_en = 0;
    gpio_config(&io);
}

static void adc_init(void)
{
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    esp_err_t ret = adc_oneshot_new_unit(&init_cfg, &s_adc1);
    if (ret != ESP_OK) {
        ESP_LOGW("OLED", "adc_oneshot_new_unit failed: %s; falling back to slider proxy",
                 esp_err_to_name(ret));
        s_adc1 = NULL;
        s_adc_cali_ok = false;
        return;
    }

    // GPIO4 and GPIO17 are on ADC1 channels (ESP32-S3: GPIO4=CH3, GPIO17=CH6)
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_11,    // 0..~3.3V目安
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (s_adc1) {
        ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc1, ADC_CHANNEL_3, &chan_cfg)); // GPIO4
        ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc1, ADC_CHANNEL_6, &chan_cfg)); // GPIO17
    }

    // キャリブレーション（未確定扱いでOK、取れたら使う）
    adc_cali_curve_fitting_config_t cal_cfg = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_11,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_curve_fitting(&cal_cfg, &s_adc_cali) == ESP_OK) {
        s_adc_cali_ok = true;
    }
}

static int read_adc_mv_gpio4_slider(void)
{
    if (s_adc1 == NULL) {
        /* Fallback: use slider ADC reading as proxy (0..1023 -> 0..3300mV)
         * This allows OLED to run when another module already owns ADC. */
        uint16_t raw = slider_read_pitchbend();
        return (raw * 3300) / 1023;
    }

    int raw = 0;
    ESP_ERROR_CHECK(adc_oneshot_read(s_adc1, ADC_CHANNEL_3, &raw)); // GPIO4

    int mv = 0;
    if (s_adc_cali_ok) {
        adc_cali_raw_to_voltage(s_adc_cali, raw, &mv);
    } else {
        mv = (raw * 3300) / 4095;
    }
    return mv; // 0..3300mV相当
}

static int read_adc_mv_gpio17_batvsense(void)
{
    if (s_adc1 == NULL) {
        /* No ADC unit available; approximate battery from slider proxy. */
        uint16_t raw = slider_read_pitchbend();
        int slider_mv = (raw * 3300) / 1023;
        return 3300 + (slider_mv * 900) / 3300; // same formula as before
    }

    int raw = 0;
    ESP_ERROR_CHECK(adc_oneshot_read(s_adc1, ADC_CHANNEL_6, &raw)); // GPIO17

    int mv = 0;
    if (s_adc_cali_ok) {
        adc_cali_raw_to_voltage(s_adc_cali, raw, &mv);
    } else {
        mv = (raw * 3300) / 4095;
    }
    return mv; // Vadc
}

// GPIO40押下ごとに: BATTERY -> EXT -> FAULT -> BATTERY
static void debug_button_update(void)
{
    int now = gpio_get_level(PIN_DBG_BUTTON);
    int64_t t = esp_timer_get_time();

    if (now != s_btn_last) {
        // デバウンス：変化から30ms以上経過で確定
        if ((t - s_btn_last_change_us) > 30000) {
            s_btn_last_change_us = t;
            s_btn_last = now;

            if (now == 0) { // falling edge = pressed
                s_dbg_mode = (power_debug_mode_t)((s_dbg_mode + 1) % 4);
            }
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
    debug_button_update();

    // 実ピン読み（デバッグ時は上書き）
    bool ext_power = (gpio_get_level(PIN_PGOOD) == 0);
    bool charging  = (gpio_get_level(PIN_CHG) == 0);

    switch (s_dbg_mode) {
        case PWR_MODE_BATTERY:
            ext_power = false;
            charging = false;   // どっちでも良いけど固定しとくと混ざらない
            break;

        case PWR_MODE_EXT_CHARGING:
            ext_power = true;
            charging  = true;
            break;

        case PWR_MODE_EXT_CHARGED:
            ext_power = true;
            charging  = false;
            break;

        case PWR_MODE_FAULT:
            p->state = PWR_STATE_FAULT;
            p->bars = 0;
            return;
    }

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
    /* Use ADC GPIO17 (battery sense) for vbat. GPIO4 is reserved for the
     * debug slider/pitch-bend and should not be used for battery calculation. */
    int vbat_mv = read_adc_mv_gpio17_batvsense(); // read real battery sense (mV)

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
    // --- Yellow area (top): Battery + OCT: 0 ---
    draw_battery_icon(u8g2, &s_pwr_ui);

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

    // ここから先でステータス表示を開始
    gpio_init_inputs();
    adc_init();

    int64_t next_update_ms = 0;

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

        vTaskDelay(pdMS_TO_TICKS(50)); // 20fps相当（軽め）
    }
}

void oled_demo_start(void)
{
    xTaskCreatePinnedToCore(oled_task, "OledDemo", 4096, NULL, 3, NULL, 0);
}
