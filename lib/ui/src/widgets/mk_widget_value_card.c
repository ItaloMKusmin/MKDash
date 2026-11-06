/*
 * Configurable value card styled after the MK tile: rounded dark block with a
 * double red slash. A value that is not current is never drawn as if it were:
 * stale values are dimmed with a warning, errors show "--" and unsupported
 * signals show "N/D".
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mk_ui_internal.h"

#define MK_CARD_PAD      18
#define MK_CARD_SLASH_W  7
#define MK_CARD_SLASH_H  26
#define MK_CARD_SLASH_SK 18
#define MK_CARD_VALUE_DY 12

typedef enum { CARD_LIVE, CARD_STALE, CARD_ERROR, CARD_UNAVAILABLE, CARD_UNSET } mk_card_state_t;

struct mk_value_card {
    lv_obj_t *obj;
    lv_obj_t *label;
    lv_obj_t *unit;
    lv_obj_t *value;
    lv_obj_t *status;
    mk_signal_id_t id;
    mk_card_state_t state;
    char last_text[16];
    int32_t value_w;
    const lv_font_t *value_font;
};

static void mk_card_draw_cb(lv_event_t *event)
{
    lv_obj_t *obj = lv_event_get_current_target(event);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    /* Double slash in the top-right corner, same lean as the monogram cuts. */
    const int32_t x = a.x2 - MK_CARD_PAD - 2 * MK_CARD_SLASH_W - MK_CARD_SLASH_SK - 8;
    const int32_t y = a.y1 + MK_CARD_PAD + MK_CARD_SLASH_H;
    lv_layer_t *layer = lv_event_get_layer(event);
    mk_ui_draw_slant(layer, x, y, MK_CARD_SLASH_W, MK_CARD_SLASH_H, MK_CARD_SLASH_SK, MK_COLOR_BRAND);
    mk_ui_draw_slant(layer, x + MK_CARD_SLASH_W + 8, y, MK_CARD_SLASH_W, MK_CARD_SLASH_H, MK_CARD_SLASH_SK,
                     MK_COLOR_BRAND);
}

/* Uses the large font when the text fits, otherwise the fallback size, so
 * a value is never clipped. */
static void mk_card_set_value(mk_value_card_t *c, const char *text)
{
    lv_point_t size;
    lv_text_get_size(&size, text, &mk_font_num_80, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    const lv_font_t *font = size.x <= c->value_w ? &mk_font_num_80 : &mk_font_num_52;
    if (font != c->value_font) {
        c->value_font = font;
        lv_obj_set_style_text_font(c->value, font, 0);
    }
    lv_label_set_text(c->value, text);
}

static void mk_card_apply_state(mk_value_card_t *c, mk_card_state_t state)
{
    if (c->state == state) {
        return;
    }
    c->state = state;
    switch (state) {
    case CARD_LIVE:
        lv_obj_set_style_text_color(c->value, MK_COLOR_TEXT, 0);
        lv_label_set_text(c->status, "");
        break;
    case CARD_STALE:
        lv_obj_set_style_text_color(c->value, MK_COLOR_TEXT_MUTED, 0);
        lv_obj_set_style_text_color(c->status, MK_COLOR_WARNING, 0);
        lv_label_set_text(c->status, "sem atualização");
        break;
    case CARD_ERROR:
        lv_obj_set_style_text_color(c->value, MK_COLOR_TEXT_MUTED, 0);
        lv_obj_set_style_text_color(c->status, MK_COLOR_BRAND, 0);
        lv_label_set_text(c->status, "leitura inválida");
        break;
    case CARD_UNAVAILABLE:
    case CARD_UNSET:
    default:
        lv_obj_set_style_text_color(c->value, MK_COLOR_TEXT_MUTED, 0);
        lv_label_set_text(c->status, "");
        break;
    }
}

mk_value_card_t *mk_value_card_create(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h,
                                      mk_signal_id_t id)
{
    mk_value_card_t *c = calloc(1, sizeof(*c));
    if (c == NULL) {
        return NULL;
    }
    c->obj = lv_obj_create(parent);
    mk_ui_style_plain(c->obj);
    lv_obj_set_style_bg_color(c->obj, MK_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(c->obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(c->obj, 22, 0);
    lv_obj_set_style_border_color(c->obj, MK_COLOR_SURFACE_ALT, 0);
    lv_obj_set_style_border_width(c->obj, 2, 0);
    /* Pressed feedback, so the long-press target is obvious. */
    lv_obj_set_style_border_color(c->obj, MK_COLOR_BRAND, LV_STATE_PRESSED);
    lv_obj_set_pos(c->obj, x, y);
    lv_obj_set_size(c->obj, w, h);
    lv_obj_add_flag(c->obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(c->obj, mk_card_draw_cb, LV_EVENT_DRAW_MAIN_END, NULL);

    c->label = lv_label_create(c->obj);
    lv_obj_set_style_text_font(c->label, &mk_font_text_20, 0);
    lv_obj_set_style_text_color(c->label, MK_COLOR_TEXT, 0);
    lv_obj_align(c->label, LV_ALIGN_TOP_LEFT, MK_CARD_PAD, MK_CARD_PAD);

    c->status = lv_label_create(c->obj);
    lv_obj_set_style_text_font(c->status, &mk_font_text_20, 0);
    lv_obj_align(c->status, LV_ALIGN_TOP_LEFT, MK_CARD_PAD, MK_CARD_PAD + 28);

    /* Value centred on the card, below the label rows; unit in the corner. */
    c->value = lv_label_create(c->obj);
    lv_obj_set_style_text_font(c->value, &mk_font_num_80, 0);
    lv_obj_set_style_text_align(c->value, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(c->value, LV_LABEL_LONG_MODE_CLIP);
    c->value_w = w - 2 * MK_CARD_PAD;
    lv_obj_set_width(c->value, c->value_w);
    lv_obj_align(c->value, LV_ALIGN_CENTER, 0, MK_CARD_VALUE_DY);

    c->unit = lv_label_create(c->obj);
    lv_obj_set_style_text_font(c->unit, &mk_font_text_20, 0);
    lv_obj_set_style_text_color(c->unit, MK_COLOR_BRAND, 0);
    lv_obj_align(c->unit, LV_ALIGN_BOTTOM_RIGHT, -MK_CARD_PAD, -MK_CARD_PAD + 2);

    c->state = CARD_UNSET;
    mk_value_card_set_signal(c, id);
    return c;
}

void mk_value_card_set_signal(mk_value_card_t *c, mk_signal_id_t id)
{
    const mk_signal_info_t *info = mk_signal_info(id);
    if (info == NULL) {
        return;
    }
    c->id = id;
    lv_label_set_text(c->label, info->label);
    lv_label_set_text(c->unit, info->unit);
    mk_card_set_value(c, "N/D");
    strcpy(c->last_text, "N/D");
    c->state = CARD_UNSET;
    mk_card_apply_state(c, CARD_UNAVAILABLE);
}

mk_signal_id_t mk_value_card_signal(const mk_value_card_t *c)
{
    return c->id;
}

lv_obj_t *mk_value_card_obj(mk_value_card_t *c)
{
    return c->obj;
}

void mk_value_card_update(mk_value_card_t *c, const mk_signal_sample_t *sample)
{
    char text[16];
    mk_card_state_t state;
    switch (sample->quality) {
    case MK_SIGNAL_VALID:
        state = CARD_LIVE;
        break;
    case MK_SIGNAL_STALE:
        state = CARD_STALE;
        break;
    case MK_SIGNAL_ERROR:
        state = CARD_ERROR;
        break;
    case MK_SIGNAL_UNAVAILABLE:
    default:
        state = CARD_UNAVAILABLE;
        break;
    }
    if (state == CARD_LIVE || state == CARD_STALE) {
        mk_ui_format_value(c->id, sample, text, sizeof(text));
    } else {
        snprintf(text, sizeof(text), "%s", state == CARD_ERROR ? "--" : "N/D");
    }
    if (strcmp(text, c->last_text) != 0) {
        strcpy(c->last_text, text);
        mk_card_set_value(c, text);
    }
    mk_card_apply_state(c, state);
}
