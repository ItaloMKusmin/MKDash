/*
 * M0 bring-up screen: color bars, touch targets at the four corners and
 * centre, live touch coordinates and memory figures. LVGL's performance
 * monitor overlay reports FPS and CPU. Replaced by the real UI in M4.
 */
#include "mk_bringup_screen.h"

#include <stdio.h>

#include "esp_heap_caps.h"
#include "lvgl.h"

#define MK_TARGET_SIZE 48

static lv_obj_t *s_touch_label;
static lv_obj_t *s_mem_label;
static lv_obj_t *s_cursor;

static void mk_bringup_mem_timer(lv_timer_t *timer)
{
    (void)timer;
    char text[128];
    snprintf(text, sizeof(text), "heap interna livre %u kB (min %u kB)   PSRAM livre %u kB",
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024U),
             (unsigned)(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL) / 1024U),
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024U));
    lv_label_set_text(s_mem_label, text);
}

static void mk_bringup_touch_cb(lv_event_t *event)
{
    lv_indev_t *indev = lv_indev_active();
    if (indev == NULL) {
        return;
    }
    lv_point_t point;
    lv_indev_get_point(indev, &point);
    lv_label_set_text_fmt(s_touch_label, "toque x=%d y=%d", (int)point.x, (int)point.y);
    lv_obj_set_pos(s_cursor, point.x - 10, point.y - 10);
    lv_obj_remove_flag(s_cursor, LV_OBJ_FLAG_HIDDEN);
    if (lv_event_get_code(event) == LV_EVENT_RELEASED) {
        lv_obj_add_flag(s_cursor, LV_OBJ_FLAG_HIDDEN);
    }
}

static void mk_bringup_add_target(lv_obj_t *parent, lv_align_t align)
{
    lv_obj_t *target = lv_obj_create(parent);
    lv_obj_remove_flag(target, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(target, MK_TARGET_SIZE, MK_TARGET_SIZE);
    lv_obj_set_style_radius(target, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(target, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(target, lv_color_hex(0xFFB020), 0);
    lv_obj_set_style_border_width(target, 3, 0);
    lv_obj_align(target, align, 0, 0);
}

void mk_bringup_screen_create(const char *version_line, bool touch_ok)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x07090C), 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    /* White, red, green, blue and grey bars: wrong byte order or channel
     * mapping shows immediately. */
    static const uint32_t colors[] = {0xFFFFFF, 0xFF0000, 0x00FF00, 0x0000FF, 0x808080};
    const int bars = (int)(sizeof(colors) / sizeof(colors[0]));
    for (int i = 0; i < bars; ++i) {
        lv_obj_t *bar = lv_obj_create(screen);
        lv_obj_remove_flag(bar, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(bar, 0, 0);
        lv_obj_set_style_border_width(bar, 0, 0);
        lv_obj_set_style_bg_color(bar, lv_color_hex(colors[i]), 0);
        lv_obj_set_size(bar, 120, 60);
        lv_obj_align(bar, LV_ALIGN_CENTER, (i - bars / 2) * 130, -150);
    }

    lv_obj_t *title = lv_label_create(screen);
    lv_obj_set_style_text_color(title, lv_color_hex(0xF5F7FA), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_label_set_text_fmt(title, "MKAuto - teste M0\n%s", version_line);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -40);

    s_touch_label = lv_label_create(screen);
    lv_obj_set_style_text_color(s_touch_label, lv_color_hex(touch_ok ? 0x21C875 : 0xFF3B30), 0);
    lv_obj_set_style_text_font(s_touch_label, &lv_font_montserrat_24, 0);
    lv_label_set_text(s_touch_label, touch_ok ? "toque nos 5 circulos" : "TOUCH INDISPONIVEL");
    lv_obj_align(s_touch_label, LV_ALIGN_CENTER, 0, 40);

    s_mem_label = lv_label_create(screen);
    lv_obj_set_style_text_color(s_mem_label, lv_color_hex(0x8C96A3), 0);
    lv_obj_align(s_mem_label, LV_ALIGN_CENTER, 0, 90);
    mk_bringup_mem_timer(NULL);
    lv_timer_create(mk_bringup_mem_timer, 1000, NULL);

    mk_bringup_add_target(screen, LV_ALIGN_TOP_LEFT);
    mk_bringup_add_target(screen, LV_ALIGN_TOP_RIGHT);
    mk_bringup_add_target(screen, LV_ALIGN_BOTTOM_LEFT);
    mk_bringup_add_target(screen, LV_ALIGN_BOTTOM_RIGHT);
    mk_bringup_add_target(screen, LV_ALIGN_CENTER);

    s_cursor = lv_obj_create(screen);
    lv_obj_remove_flag(s_cursor, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_cursor, 20, 20);
    lv_obj_set_style_radius(s_cursor, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(s_cursor, 0, 0);
    lv_obj_set_style_bg_color(s_cursor, lv_color_hex(0xFF1F24), 0);
    lv_obj_add_flag(s_cursor, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_IGNORE_LAYOUT);

    lv_obj_add_flag(screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(screen, mk_bringup_touch_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(screen, mk_bringup_touch_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(screen, mk_bringup_touch_cb, LV_EVENT_RELEASED, NULL);
}
