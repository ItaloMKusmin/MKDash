#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "lvgl.h"

#define MK_BSP_LCD_H_RES 1024
#define MK_BSP_LCD_V_RES 600

/*
 * Starts backlight PWM, MIPI-DSI panel, GT911 touch and the LVGL task.
 * The backlight stays off; call mk_bsp_backlight_set() after the first screen
 * is built. Touch failure is logged and does not prevent the display from
 * starting (mk_bsp_touch_ok() reports it).
 */
esp_err_t mk_bsp_display_start(lv_display_t **out_display);

bool mk_bsp_touch_ok(void);

/* All LVGL calls outside the LVGL task must hold this lock. */
bool mk_bsp_display_lock(uint32_t timeout_ms);
void mk_bsp_display_unlock(void);

/* 0 = off, 100 = maximum. Values above 100 are clamped. */
esp_err_t mk_bsp_backlight_set(uint8_t percent);
