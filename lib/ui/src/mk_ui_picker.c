/*
 * Modal list of every catalog signal, used to choose what a card shows.
 * Closes on selection, on "Cancelar" or after inactivity.
 */
#include "mk_ui_internal.h"

#define MK_PICKER_TIMEOUT_MS 20000
#define MK_PICKER_W          720
#define MK_PICKER_H          560
#define MK_PICKER_BTN_W      330
#define MK_PICKER_BTN_H      56

static lv_obj_t *s_overlay;
static lv_timer_t *s_timeout;
static mk_picker_done_cb_t s_done;
static void *s_user;

void mk_picker_close(void)
{
    if (s_timeout != NULL) {
        lv_timer_delete(s_timeout);
        s_timeout = NULL;
    }
    if (s_overlay != NULL) {
        /* Usually called from an event of a child; deleting it synchronously
         * would free the object LVGL is still dispatching to. */
        lv_obj_delete_async(s_overlay);
        s_overlay = NULL;
    }
}

bool mk_picker_is_open(void)
{
    return s_overlay != NULL;
}

static void mk_picker_timeout_cb(lv_timer_t *timer)
{
    (void)timer;
    mk_picker_close();
}

static void mk_picker_select_cb(lv_event_t *event)
{
    const mk_signal_id_t id = (mk_signal_id_t)(intptr_t)lv_event_get_user_data(event);
    mk_picker_done_cb_t done = s_done;
    void *user = s_user;
    mk_picker_close();
    if (done != NULL) {
        done(id, user);
    }
}

static void mk_picker_cancel_cb(lv_event_t *event)
{
    (void)event;
    mk_picker_close();
}

static void mk_picker_activity_cb(lv_event_t *event)
{
    (void)event;
    if (s_timeout != NULL) {
        lv_timer_reset(s_timeout);
    }
}

static lv_obj_t *mk_picker_button(lv_obj_t *parent, const char *text, bool selected)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, MK_PICKER_BTN_W, MK_PICKER_BTN_H);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_bg_color(btn, selected ? MK_COLOR_BRAND : MK_COLOR_SURFACE_ALT, 0);
    lv_obj_set_style_bg_color(btn, MK_COLOR_TEXT_MUTED, LV_STATE_PRESSED);
    lv_obj_t *label = lv_label_create(btn);
    lv_obj_set_style_text_font(label, &mk_font_text_20, 0);
    lv_obj_set_style_text_color(label, MK_COLOR_TEXT, 0);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return btn;
}

void mk_picker_open(mk_signal_id_t current, mk_picker_done_cb_t done, void *user)
{
    mk_picker_close();
    s_done = done;
    s_user = user;

    s_overlay = lv_obj_create(lv_layer_top());
    mk_ui_style_plain(s_overlay);
    lv_obj_set_size(s_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_overlay, MK_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_80, 0);
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_CLICKABLE);   /* swallow touches behind */
    lv_obj_add_event_cb(s_overlay, mk_picker_activity_cb, LV_EVENT_PRESSED, NULL);

    lv_obj_t *panel = lv_obj_create(s_overlay);
    mk_ui_style_plain(panel);
    lv_obj_set_size(panel, MK_PICKER_W, MK_PICKER_H);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, MK_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(panel, 14, 0);
    lv_obj_set_style_pad_all(panel, 20, 0);
    lv_obj_add_event_cb(panel, mk_picker_activity_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(panel, mk_picker_activity_cb, LV_EVENT_SCROLL, NULL);

    lv_obj_t *title = lv_label_create(panel);
    lv_obj_set_style_text_font(title, &mk_font_text_28, 0);
    lv_obj_set_style_text_color(title, MK_COLOR_TEXT, 0);
    lv_label_set_text(title, "Escolha o dado deste campo");
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *list = lv_obj_create(panel);
    mk_ui_style_plain(list);
    lv_obj_set_size(list, LV_PCT(100), MK_PICKER_H - 40 - 34 - 16 - MK_PICKER_BTN_H - 16);
    lv_obj_align(list, LV_ALIGN_TOP_LEFT, 0, 50);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(list, 10, 0);
    lv_obj_set_style_pad_column(list, 20, 0);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(list, mk_picker_activity_cb, LV_EVENT_SCROLL, NULL);

    for (int i = 0; i < MK_SIG_COUNT; ++i) {
        const mk_signal_info_t *info = mk_signal_info((mk_signal_id_t)i);
        lv_obj_t *btn = mk_picker_button(list, info->label, i == (int)current);
        lv_obj_add_event_cb(btn, mk_picker_select_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    lv_obj_t *cancel = mk_picker_button(panel, "Cancelar", false);
    lv_obj_set_width(cancel, 200);
    lv_obj_align(cancel, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_add_event_cb(cancel, mk_picker_cancel_cb, LV_EVENT_CLICKED, NULL);

    s_timeout = lv_timer_create(mk_picker_timeout_cb, MK_PICKER_TIMEOUT_MS, NULL);
}
