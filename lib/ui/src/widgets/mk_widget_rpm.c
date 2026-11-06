/*
 * MKAuto tachometer: a rising ramp of slanted segments echoing the diagonal
 * cuts of the MK monogram. Drawn by one object so an update costs a single
 * invalidation. Zones stay tinted when unlit; above the shift threshold the
 * lit segments flash.
 */
#include <stdlib.h>

#include "mk_ui_internal.h"

#define MK_RPM_SEGMENTS   40
#define MK_RPM_GAP        7
#define MK_RPM_MIN_H      36
#define MK_RPM_SKEW_PCT   45   /* horizontal lean as % of segment height */
#define MK_RPM_FLASH_MS   110

typedef enum { LIT_OFF, LIT_ZONE, LIT_STALE, LIT_FLASH_A, LIT_FLASH_B } mk_lit_mode_t;

struct mk_rpm_widget {
    lv_obj_t *obj;
    float rpm_per_segment;
    uint16_t rpm_warning;
    uint16_t rpm_shift;
    int lit;
    mk_lit_mode_t mode;
};

static lv_color_t mk_rpm_segment_color(const mk_rpm_widget_t *w, int i)
{
    const bool hot = (float)i * w->rpm_per_segment >= w->rpm_warning;
    if (i >= w->lit || w->mode == LIT_OFF) {
        return hot ? lv_color_mix(MK_COLOR_BRAND, MK_COLOR_SURFACE_ALT, 70) : MK_COLOR_SURFACE_ALT;
    }
    switch (w->mode) {
    case LIT_STALE:   return MK_COLOR_TEXT_MUTED;
    case LIT_FLASH_A: return MK_COLOR_TEXT;
    case LIT_FLASH_B: return MK_COLOR_BRAND;
    case LIT_ZONE:
    default:          return hot ? MK_COLOR_BRAND : MK_COLOR_TEXT;
    }
}

static int32_t mk_rpm_segment_width(int32_t width, int32_t height)
{
    const int32_t max_skew = height * MK_RPM_SKEW_PCT / 100;
    return (width - max_skew - (MK_RPM_SEGMENTS - 1) * MK_RPM_GAP) / MK_RPM_SEGMENTS;
}

static void mk_rpm_draw_cb(lv_event_t *event)
{
    mk_rpm_widget_t *w = lv_event_get_user_data(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    lv_area_t area;
    lv_obj_get_coords(w->obj, &area);

    const int32_t height = lv_area_get_height(&area);
    const int32_t seg_w = mk_rpm_segment_width(lv_area_get_width(&area), height);
    for (int i = 0; i < MK_RPM_SEGMENTS; ++i) {
        const int32_t h = MK_RPM_MIN_H + (height - MK_RPM_MIN_H) * i / (MK_RPM_SEGMENTS - 1);
        const int32_t x = area.x1 + i * (seg_w + MK_RPM_GAP);
        mk_ui_draw_slant(layer, x, area.y2, seg_w, h, h * MK_RPM_SKEW_PCT / 100,
                         mk_rpm_segment_color(w, i));
    }
}

mk_rpm_widget_t *mk_rpm_widget_create(lv_obj_t *parent, int32_t x, int32_t y, int32_t width, int32_t height,
                                      uint16_t rpm_max, uint16_t rpm_warning, uint16_t rpm_shift)
{
    mk_rpm_widget_t *w = calloc(1, sizeof(*w));
    if (w == NULL) {
        return NULL;
    }
    w->rpm_per_segment = (float)rpm_max / MK_RPM_SEGMENTS;
    w->rpm_warning = rpm_warning;
    w->rpm_shift = rpm_shift;
    w->lit = -1;

    w->obj = lv_obj_create(parent);
    mk_ui_style_plain(w->obj);
    lv_obj_set_pos(w->obj, x, y);
    lv_obj_set_size(w->obj, width, height);
    lv_obj_add_event_cb(w->obj, mk_rpm_draw_cb, LV_EVENT_DRAW_MAIN, w);

    /* Thousands marks under the matching segment. */
    const int32_t seg_w = mk_rpm_segment_width(width, height);
    for (int k = 0; k * 1000 <= rpm_max; ++k) {
        int seg = (int)((float)(k * 1000) / w->rpm_per_segment);
        if (seg >= MK_RPM_SEGMENTS) {
            seg = MK_RPM_SEGMENTS - 1;
        }
        lv_obj_t *tick = lv_label_create(parent);
        lv_obj_set_style_text_font(tick, &mk_font_text_20, 0);
        lv_obj_set_style_text_color(tick, k * 1000 >= rpm_warning ? MK_COLOR_BRAND : MK_COLOR_TEXT_MUTED, 0);
        lv_label_set_text_fmt(tick, "%d", k);
        lv_obj_set_pos(tick, x + seg * (seg_w + MK_RPM_GAP), y + height + 8);
    }
    return w;
}

bool mk_rpm_widget_update(mk_rpm_widget_t *w, const mk_signal_sample_t *rpm, uint32_t now_ms)
{
    const bool live = rpm->quality == MK_SIGNAL_VALID;
    const bool shown = live || rpm->quality == MK_SIGNAL_STALE;
    const bool shift = live && rpm->value >= w->rpm_shift;

    int lit = shown ? (int)(rpm->value / w->rpm_per_segment + 0.5f) : 0;
    if (lit > MK_RPM_SEGMENTS) {
        lit = MK_RPM_SEGMENTS;
    }
    mk_lit_mode_t mode = LIT_OFF;
    if (shown) {
        if (!live) {
            mode = LIT_STALE;
        } else if (shift) {
            mode = ((now_ms / MK_RPM_FLASH_MS) % 2U) == 0U ? LIT_FLASH_A : LIT_FLASH_B;
        } else {
            mode = LIT_ZONE;
        }
    }
    if (lit != w->lit || mode != w->mode) {
        w->lit = lit;
        w->mode = mode;
        lv_obj_invalidate(w->obj);
    }
    return shift;
}
