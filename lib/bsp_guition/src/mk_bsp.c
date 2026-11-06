/*
 * GUITION JC1060P470C_I_W_Y board support.
 *
 * Ported from the BSP validated on this board by the SMI-BULK project
 * (lib/bsp_jc1060p470c), which in turn derives from the official GUITION demo.
 * Only the display, touch and backlight are kept here.
 */
#include "mk_bsp.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "esp_cache.h"
#include "esp_check.h"
#include "esp_lcd_jd9165.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_ldo_regulator.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "mk_bsp_panel_init.h"

static const char *TAG = "mk_bsp";

/* Board pins (GUITION JC1060P470C_I_W_Y, confirmed by SMI-BULK on the same unit). */
#define MK_BSP_I2C_PORT         1
#define MK_BSP_I2C_SDA          GPIO_NUM_7
#define MK_BSP_I2C_SCL          GPIO_NUM_8
#define MK_BSP_I2C_HZ           400000
#define MK_BSP_TOUCH_RST        GPIO_NUM_22
#define MK_BSP_TOUCH_INT        GPIO_NUM_21
#define MK_BSP_BACKLIGHT        GPIO_NUM_23
#define MK_BSP_LCD_RST          CONFIG_MK_BSP_LCD_RST_GPIO

/* MIPI-DSI: 2 lanes at 750 Mbps, 52 MHz pixel clock, PHY powered by LDO_VO3. */
#define MK_BSP_DSI_LANES        2
#define MK_BSP_DSI_LANE_MBPS    750
#define MK_BSP_DPI_CLOCK_MHZ    52
#define MK_BSP_DSI_PHY_LDO_CHAN 3
#define MK_BSP_DSI_PHY_LDO_MV   2500

#if CONFIG_MK_BSP_LCD_PANEL_NEW
#define MK_BSP_HSYNC_PW 24
#define MK_BSP_HSYNC_BP 136
#define MK_BSP_VSYNC_PW 2
#define MK_BSP_VSYNC_BP 21
#define MK_BSP_PANEL_INIT jd9165_init_painel_novo
#else
#define MK_BSP_HSYNC_PW 20
#define MK_BSP_HSYNC_BP 160
#define MK_BSP_VSYNC_PW 10
#define MK_BSP_VSYNC_BP 23
#define MK_BSP_PANEL_INIT jd9165_init_painel_antigo
#endif
#define MK_BSP_HSYNC_FP 160
#define MK_BSP_VSYNC_FP 12

/* LVGL draws into a partial buffer in internal DMA RAM; the panel keeps one
 * full frame buffer in PSRAM. Two 100 kB internal buffers do not fit. */
#define MK_BSP_DRAW_BUF_PX      (MK_BSP_LCD_H_RES * 50)
/* The default 7 kB LVGL task stack overflows inside vsnprintf on larger screens. */
#define MK_BSP_LVGL_STACK       16384

#define MK_BSP_LEDC_TIMER       LEDC_TIMER_1
#define MK_BSP_LEDC_CHANNEL     LEDC_CHANNEL_1
#define MK_BSP_LEDC_MAX_DUTY    1023

static bool s_touch_ok;

static esp_err_t mk_bsp_backlight_init(void)
{
    const ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = MK_BSP_LEDC_TIMER,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    const ledc_channel_config_t channel = {
        .gpio_num = MK_BSP_BACKLIGHT,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = MK_BSP_LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = MK_BSP_LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG, "LEDC timer");
    return ledc_channel_config(&channel);
}

esp_err_t mk_bsp_backlight_set(uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }
    /* Active high: duty 0 is off. */
    const uint32_t duty = (MK_BSP_LEDC_MAX_DUTY * (uint32_t)percent) / 100U;
    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, MK_BSP_LEDC_CHANNEL, duty), TAG, "duty");
    return ledc_update_duty(LEDC_LOW_SPEED_MODE, MK_BSP_LEDC_CHANNEL);
}

static esp_err_t mk_bsp_dsi_phy_power(void)
{
    static esp_ldo_channel_handle_t s_ldo;
    if (s_ldo != NULL) {
        return ESP_OK;
    }
    const esp_ldo_channel_config_t cfg = {
        .chan_id = MK_BSP_DSI_PHY_LDO_CHAN,
        .voltage_mv = MK_BSP_DSI_PHY_LDO_MV,
    };
    return esp_ldo_acquire_channel(&cfg, &s_ldo);
}

#if CONFIG_MK_BSP_LCD_TEST_PATTERN
/* Phase 1 bypasses the frame buffer (link/init check); phase 2 goes through
 * frame buffer, cache flush and DMA. */
static void mk_bsp_test_pattern(esp_lcd_panel_handle_t panel)
{
    mk_bsp_backlight_set(100);
    ESP_LOGW(TAG, "test pattern 1/2: DSI hardware bars");
    esp_lcd_dpi_panel_set_pattern(panel, MIPI_DSI_PATTERN_BAR_VERTICAL);
    vTaskDelay(pdMS_TO_TICKS(4000));
    esp_lcd_dpi_panel_set_pattern(panel, MIPI_DSI_PATTERN_NONE);

    uint16_t *fb = NULL;
    if (esp_lcd_dpi_panel_get_frame_buffer(panel, 1, (void **)&fb) != ESP_OK || fb == NULL) {
        ESP_LOGE(TAG, "test pattern 2/2: no frame buffer");
        return;
    }
    static const uint16_t colors[] = {0xFFFF, 0xF800, 0x07E0, 0x001F, 0x0000};
    const int bands = (int)(sizeof(colors) / sizeof(colors[0]));
    const int band_w = MK_BSP_LCD_H_RES / bands;
    for (int y = 0; y < MK_BSP_LCD_V_RES; ++y) {
        uint16_t *line = fb + (size_t)y * MK_BSP_LCD_H_RES;
        for (int x = 0; x < MK_BSP_LCD_H_RES; ++x) {
            const int band = x / band_w;
            line[x] = colors[band < bands ? band : bands - 1];
        }
    }
    esp_cache_msync(fb, (size_t)MK_BSP_LCD_H_RES * MK_BSP_LCD_V_RES * sizeof(uint16_t),
                    ESP_CACHE_MSYNC_FLAG_DIR_C2M);
    ESP_LOGW(TAG, "test pattern 2/2: frame buffer bars white/red/green/blue/black");
    vTaskDelay(pdMS_TO_TICKS(4000));
    mk_bsp_backlight_set(0);
}
#endif

static esp_err_t mk_bsp_panel_new(esp_lcd_panel_io_handle_t *out_io, esp_lcd_panel_handle_t *out_panel)
{
    esp_err_t ret = ESP_OK;
    esp_lcd_dsi_bus_handle_t bus = NULL;
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_handle_t panel = NULL;

    const esp_lcd_dsi_bus_config_t bus_cfg = {
        .bus_id = 0,
        .num_data_lanes = MK_BSP_DSI_LANES,
        .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        .lane_bit_rate_mbps = MK_BSP_DSI_LANE_MBPS,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_dsi_bus(&bus_cfg, &bus), TAG, "DSI bus");

    const esp_lcd_dbi_io_config_t dbi_cfg = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_dbi(bus, &dbi_cfg, &io), fail, TAG, "panel IO");

    esp_lcd_dpi_panel_config_t dpi_cfg = {
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = MK_BSP_DPI_CLOCK_MHZ,
        .virtual_channel = 0,
        .pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565,
        .num_fbs = 1,
        .video_timing = {
            .h_size = MK_BSP_LCD_H_RES,
            .v_size = MK_BSP_LCD_V_RES,
            .hsync_pulse_width = MK_BSP_HSYNC_PW,
            .hsync_back_porch = MK_BSP_HSYNC_BP,
            .hsync_front_porch = MK_BSP_HSYNC_FP,
            .vsync_pulse_width = MK_BSP_VSYNC_PW,
            .vsync_back_porch = MK_BSP_VSYNC_BP,
            .vsync_front_porch = MK_BSP_VSYNC_FP,
        },
        .flags.use_dma2d = true,
    };
    const jd9165_vendor_config_t vendor_cfg = {
        .init_cmds = MK_BSP_PANEL_INIT,
        .init_cmds_size = (uint16_t)(sizeof(MK_BSP_PANEL_INIT) / sizeof(MK_BSP_PANEL_INIT[0])),
        .mipi_config = {.dsi_bus = bus, .dpi_config = &dpi_cfg},
    };
    const esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = MK_BSP_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = (void *)&vendor_cfg,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_jd9165(io, &panel_cfg, &panel), fail, TAG, "JD9165");
    ESP_GOTO_ON_ERROR(esp_lcd_panel_reset(panel), fail, TAG, "panel reset");
    ESP_GOTO_ON_ERROR(esp_lcd_panel_init(panel), fail, TAG, "panel init");

    ESP_LOGI(TAG, "panel %s, reset GPIO%d, %dx%d", CONFIG_MK_BSP_LCD_PANEL_NEW ? "new" : "old",
             MK_BSP_LCD_RST, MK_BSP_LCD_H_RES, MK_BSP_LCD_V_RES);
    *out_io = io;
    *out_panel = panel;
    return ESP_OK;

fail:
    if (panel != NULL) {
        esp_lcd_panel_del(panel);
    }
    if (io != NULL) {
        esp_lcd_panel_io_del(io);
    }
    esp_lcd_del_dsi_bus(bus);
    return ret;
}

static esp_err_t mk_bsp_touch_new(esp_lcd_touch_handle_t *out_touch)
{
    i2c_master_bus_handle_t bus = NULL;
    const i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .sda_io_num = MK_BSP_I2C_SDA,
        .scl_io_num = MK_BSP_I2C_SCL,
        .i2c_port = MK_BSP_I2C_PORT,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &bus), TAG, "I2C bus");

    /* Without driver_data the component skips GT911 address selection and the
     * config read fails. It must outlive this call. */
    static esp_lcd_touch_io_gt911_config_t s_gt911_cfg = {
        .dev_addr = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS,
    };
    const esp_lcd_touch_config_t touch_cfg = {
        .driver_data = &s_gt911_cfg,
        .x_max = MK_BSP_LCD_H_RES,
        .y_max = MK_BSP_LCD_V_RES,
        .rst_gpio_num = MK_BSP_TOUCH_RST,
        .int_gpio_num = MK_BSP_TOUCH_INT,
        .levels = {.reset = 0, .interrupt = 0},
    };
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_i2c_config_t io_cfg = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    io_cfg.scl_speed_hz = MK_BSP_I2C_HZ;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(bus, &io_cfg, &io), TAG, "touch IO");
    return esp_lcd_touch_new_i2c_gt911(io, &touch_cfg, out_touch);
}

esp_err_t mk_bsp_display_start(lv_display_t **out_display)
{
    ESP_RETURN_ON_FALSE(out_display != NULL, ESP_ERR_INVALID_ARG, TAG, "out_display");

    ESP_RETURN_ON_ERROR(mk_bsp_backlight_init(), TAG, "backlight");
    ESP_RETURN_ON_ERROR(mk_bsp_dsi_phy_power(), TAG, "DSI PHY power");

    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_handle_t panel = NULL;
    ESP_RETURN_ON_ERROR(mk_bsp_panel_new(&io, &panel), TAG, "panel");
#if CONFIG_MK_BSP_LCD_TEST_PATTERN
    mk_bsp_test_pattern(panel);
#endif

    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_stack = MK_BSP_LVGL_STACK;
    ESP_RETURN_ON_ERROR(lvgl_port_init(&port_cfg), TAG, "LVGL port");

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io,
        .panel_handle = panel,
        .buffer_size = MK_BSP_DRAW_BUF_PX,
        .double_buffer = false,
        .hres = MK_BSP_LCD_H_RES,
        .vres = MK_BSP_LCD_V_RES,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {.buff_dma = true},
    };
    const lvgl_port_display_dsi_cfg_t dsi_cfg = {0};
    /* esp_lvgl_port calls swap_xy unconditionally; JD9165 rejects it with a
     * harmless error log at rotation 0. Silence only during creation. */
    esp_log_level_set("lcd_panel", ESP_LOG_NONE);
    lv_display_t *display = lvgl_port_add_disp_dsi(&disp_cfg, &dsi_cfg);
    esp_log_level_set("lcd_panel", ESP_LOG_INFO);
    ESP_RETURN_ON_FALSE(display != NULL, ESP_FAIL, TAG, "LVGL display");

    esp_lcd_touch_handle_t touch = NULL;
    const esp_err_t touch_err = mk_bsp_touch_new(&touch);
    if (touch_err == ESP_OK) {
        const lvgl_port_touch_cfg_t touch_cfg = {.disp = display, .handle = touch};
        s_touch_ok = lvgl_port_add_touch(&touch_cfg) != NULL;
    }
    if (!s_touch_ok) {
        ESP_LOGE(TAG, "GT911 touch unavailable: %s", esp_err_to_name(touch_err));
    }

    *out_display = display;
    return ESP_OK;
}

bool mk_bsp_touch_ok(void)
{
    return s_touch_ok;
}

bool mk_bsp_display_lock(uint32_t timeout_ms)
{
    return lvgl_port_lock(timeout_ms);
}

void mk_bsp_display_unlock(void)
{
    lvgl_port_unlock();
}
