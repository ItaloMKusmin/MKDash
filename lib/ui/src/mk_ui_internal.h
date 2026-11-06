#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"
#include "mk_telemetry.h"

/* Theme tokens, ARCHITECTURE.md 11.2, revised from the MK monogram
 * (assets/brand/mk_logo_source.png). Screens use only these. */
#define MK_COLOR_BG          lv_color_hex(0x050506)
#define MK_COLOR_SURFACE     lv_color_hex(0x0F0E11)   /* monogram tile */
#define MK_COLOR_SURFACE_ALT lv_color_hex(0x1C1B20)
#define MK_COLOR_BRAND       lv_color_hex(0xF91315)   /* monogram red */
#define MK_COLOR_TEXT        lv_color_hex(0xF5F5F7)
#define MK_COLOR_TEXT_MUTED  lv_color_hex(0x7D7B84)
#define MK_COLOR_WARNING     lv_color_hex(0xFFB020)

/* Layout is designed for the 1024 x 600 GUITION panel. */
#define MK_UI_SCREEN_W       1024
#define MK_UI_SCREEN_H       600

LV_FONT_DECLARE(mk_font_num_150)
LV_FONT_DECLARE(mk_font_num_80)
LV_FONT_DECLARE(mk_font_num_52)
LV_IMAGE_DECLARE(mk_logo_splash);
LV_IMAGE_DECLARE(mk_logo_img);
LV_FONT_DECLARE(mk_font_text_20)
LV_FONT_DECLARE(mk_font_text_28)

/* Removes default theme decoration from a plain container. */
void mk_ui_style_plain(lv_obj_t *obj);

/* Filled parallelogram leaning right: bottom edge from (x, y_bottom) with
 * width w, top edge shifted right by skew, height h. */
void mk_ui_draw_slant(lv_layer_t *layer, int32_t x, int32_t y_bottom, int32_t w, int32_t h, int32_t skew,
                      lv_color_t color);

/* Formats a sample for display. Returns false when the value must not be
 * shown as current (stale, error, unavailable). */
bool mk_ui_format_value(mk_signal_id_t id, const mk_signal_sample_t *sample, char *out, size_t out_len);

/* ---- Tachometer ---- */
typedef struct mk_rpm_widget mk_rpm_widget_t;
mk_rpm_widget_t *mk_rpm_widget_create(lv_obj_t *parent, int32_t x, int32_t y, int32_t width, int32_t height,
                                      uint16_t rpm_max, uint16_t rpm_warning, uint16_t rpm_shift);
/* Returns true while the shift alert is active. */
bool mk_rpm_widget_update(mk_rpm_widget_t *widget, const mk_signal_sample_t *rpm, uint32_t now_ms);

/* ---- Value card ---- */
typedef struct mk_value_card mk_value_card_t;
mk_value_card_t *mk_value_card_create(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h,
                                      mk_signal_id_t id);
void mk_value_card_set_signal(mk_value_card_t *card, mk_signal_id_t id);
mk_signal_id_t mk_value_card_signal(const mk_value_card_t *card);
lv_obj_t *mk_value_card_obj(mk_value_card_t *card);
void mk_value_card_update(mk_value_card_t *card, const mk_signal_sample_t *sample);

/* ---- Signal picker ---- */
typedef void (*mk_picker_done_cb_t)(mk_signal_id_t chosen, void *user);
void mk_picker_open(mk_signal_id_t current, mk_picker_done_cb_t done, void *user);
bool mk_picker_is_open(void);
void mk_picker_close(void);
