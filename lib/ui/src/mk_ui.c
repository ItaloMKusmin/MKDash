/*
 * MKAuto dashboard: slanted tachometer, RPM and speed around the MK monogram with
 * the gear below it, and four configurable cards; the monogram also opens as splash. Reads telemetry only through snapshots, from
 * the LVGL task.
 */
#include "mk_ui.h"

#include <stdio.h>
#include <string.h>

#include "esp_check.h"
#include "esp_timer.h"
#include "mk_ui_internal.h"

static const char *TAG = "mk_ui";

#define MK_UI_REFRESH_MS     33
#define MK_UI_MARGIN         24
#define MK_UI_TACH_Y         22
#define MK_UI_TACH_H         108
#define MK_UI_NUM_Y          186
#define MK_UI_NUM_W          416
#define MK_UI_CAPTION_Y      302
#define MK_UI_LOGO_Y         166
#define MK_UI_GEAR_Y         282
#define MK_UI_SPLASH_MS      1500
#define MK_UI_SPLASH_FADE_MS 400
#define MK_UI_CARD_Y         374
#define MK_UI_CARD_H         204
#define MK_UI_CARD_GAP       12
#define MK_UI_LOCK_SPEED_KMH 5.0f
#define MK_UI_TOAST_MS       2500

typedef enum { CAPTION_NORMAL, CAPTION_SHIFT, CAPTION_NO_SIGNAL } mk_caption_t;

typedef struct {
    mk_ui_config_t cfg;
    mk_rpm_widget_t *rpm;
    lv_obj_t *rpm_value;
    lv_obj_t *rpm_caption;
    lv_obj_t *speed_value;
    lv_obj_t *gear_value;
    lv_obj_t *toast;
    lv_timer_t *toast_timer;
    mk_value_card_t *cards[MK_UI_CARD_COUNT];
    bool moving;
    char rpm_text[12];
    char speed_text[12];
    char gear_text[4];
    int gear_color;
    int rpm_color;
    int speed_color;
    mk_caption_t caption;
} mk_ui_t;

static mk_ui_t s_ui;

void mk_ui_style_plain(lv_obj_t *obj)
{
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

void mk_ui_draw_slant(lv_layer_t *layer, int32_t x, int32_t y_bottom, int32_t w, int32_t h, int32_t skew,
                      lv_color_t color)
{
    lv_draw_triangle_dsc_t dsc;
    lv_draw_triangle_dsc_init(&dsc);
    dsc.color = color;
    dsc.opa = LV_OPA_COVER;
    const int32_t top = y_bottom - h;
    /* Two triangles: (BL, BR, TR) and (BL, TR, TL). */
    dsc.p[0].x = x;            dsc.p[0].y = y_bottom;
    dsc.p[1].x = x + w;        dsc.p[1].y = y_bottom;
    dsc.p[2].x = x + w + skew; dsc.p[2].y = top;
    lv_draw_triangle(layer, &dsc);
    dsc.p[1].x = x + w + skew; dsc.p[1].y = top;
    dsc.p[2].x = x + skew;     dsc.p[2].y = top;
    lv_draw_triangle(layer, &dsc);
}

bool mk_ui_format_value(mk_signal_id_t id, const mk_signal_sample_t *sample, char *out, size_t out_len)
{
    const mk_signal_info_t *info = mk_signal_info(id);
    if (info == NULL || out == NULL || out_len == 0) {
        return false;
    }
    if (sample->quality != MK_SIGNAL_VALID && sample->quality != MK_SIGNAL_STALE) {
        snprintf(out, out_len, "N/D");
        return false;
    }
    snprintf(out, out_len, "%.*f", (int)info->decimals, (double)sample->value);
    return sample->quality == MK_SIGNAL_VALID;
}

static uint32_t mk_ui_now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static lv_obj_t *mk_ui_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, const char *text)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_label_set_text(label, text);
    return label;
}

static void mk_ui_toast_hide_cb(lv_timer_t *timer)
{
    (void)timer;
    lv_obj_add_flag(s_ui.toast, LV_OBJ_FLAG_HIDDEN);
    s_ui.toast_timer = NULL;
}

static void mk_ui_show_toast(const char *text)
{
    lv_label_set_text(s_ui.toast, text);
    lv_obj_remove_flag(s_ui.toast, LV_OBJ_FLAG_HIDDEN);
    if (s_ui.toast_timer != NULL) {
        lv_timer_reset(s_ui.toast_timer);
    } else {
        s_ui.toast_timer = lv_timer_create(mk_ui_toast_hide_cb, MK_UI_TOAST_MS, NULL);
        lv_timer_set_repeat_count(s_ui.toast_timer, 1);
    }
}

static void mk_ui_picker_done(mk_signal_id_t chosen, void *user)
{
    const int index = (int)(intptr_t)user;
    if (index < 0 || index >= MK_UI_CARD_COUNT) {
        return;
    }
    mk_value_card_set_signal(s_ui.cards[index], chosen);
    s_ui.cfg.cards[index] = chosen;
    if (s_ui.cfg.on_cards_changed != NULL) {
        s_ui.cfg.on_cards_changed(s_ui.cfg.cards);
    }
}

static void mk_ui_card_long_press_cb(lv_event_t *event)
{
    const int index = (int)(intptr_t)lv_event_get_user_data(event);
    /* ARCHITECTURE.md 11.6: no configuration while the vehicle moves. */
    if (s_ui.moving) {
        mk_ui_show_toast("Configuração disponível apenas com o veículo parado");
        return;
    }
    mk_picker_open(s_ui.cfg.cards[index], mk_ui_picker_done, (void *)(intptr_t)index);
}

/* Large readout with a red caption, as in "6250 / RPM". */
static lv_obj_t *mk_ui_readout(lv_obj_t *screen, int32_t x, lv_text_align_t align, const char *caption,
                               lv_obj_t **out_caption)
{
    lv_obj_t *value = mk_ui_label(screen, &mk_font_num_150, MK_COLOR_TEXT, "--");
    lv_obj_set_style_text_align(value, align, 0);
    lv_obj_set_size(value, MK_UI_NUM_W, LV_SIZE_CONTENT);
    lv_obj_set_pos(value, x, MK_UI_NUM_Y);

    lv_obj_t *cap = mk_ui_label(screen, &mk_font_text_28, MK_COLOR_BRAND, caption);
    lv_obj_set_style_text_align(cap, align, 0);
    lv_obj_set_style_pad_hor(cap, 10, 0);
    lv_obj_set_style_radius(cap, 6, 0);
    lv_obj_set_style_bg_color(cap, MK_COLOR_BRAND, 0);
    lv_obj_set_style_bg_opa(cap, LV_OPA_TRANSP, 0);
    lv_obj_set_pos(cap, x, MK_UI_CAPTION_Y);
    lv_obj_set_width(cap, MK_UI_NUM_W);
    if (out_caption != NULL) {
        *out_caption = cap;
    }
    return value;
}

static void mk_ui_build(lv_obj_t *screen)
{
    s_ui.rpm = mk_rpm_widget_create(screen, MK_UI_MARGIN, MK_UI_TACH_Y, MK_UI_SCREEN_W - 2 * MK_UI_MARGIN,
                                    MK_UI_TACH_H, s_ui.cfg.rpm_max, s_ui.cfg.rpm_warning, s_ui.cfg.rpm_shift);

    s_ui.rpm_value = mk_ui_readout(screen, MK_UI_MARGIN, LV_TEXT_ALIGN_CENTER, "RPM", &s_ui.rpm_caption);
    s_ui.speed_value = mk_ui_readout(screen, MK_UI_SCREEN_W - MK_UI_MARGIN - MK_UI_NUM_W, LV_TEXT_ALIGN_CENTER,
                                     "KM/H", NULL);
    strcpy(s_ui.rpm_text, "--");
    strcpy(s_ui.speed_text, "--");

    /* Centre column: monogram, and the gear as a bare white digit below it. */
    lv_obj_t *logo = lv_image_create(screen);
    lv_image_set_src(logo, &mk_logo_img);
    lv_obj_set_pos(logo, (MK_UI_SCREEN_W - mk_logo_img.header.w) / 2, MK_UI_LOGO_Y);
    s_ui.gear_value = mk_ui_label(screen, &mk_font_num_80, MK_COLOR_TEXT_MUTED, "-");
    lv_obj_set_style_text_align(s_ui.gear_value, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_ui.gear_value, 120);
    lv_obj_set_pos(s_ui.gear_value, (MK_UI_SCREEN_W - 120) / 2, MK_UI_GEAR_Y);
    strcpy(s_ui.gear_text, "-");

    const int32_t card_w = (MK_UI_SCREEN_W - 2 * MK_UI_MARGIN - (MK_UI_CARD_COUNT - 1) * MK_UI_CARD_GAP)
                           / MK_UI_CARD_COUNT;
    for (int i = 0; i < MK_UI_CARD_COUNT; ++i) {
        mk_value_card_t *card = mk_value_card_create(screen, MK_UI_MARGIN + i * (card_w + MK_UI_CARD_GAP),
                                                     MK_UI_CARD_Y, card_w, MK_UI_CARD_H, s_ui.cfg.cards[i]);
        s_ui.cards[i] = card;
        if (card != NULL) {
            lv_obj_add_event_cb(mk_value_card_obj(card), mk_ui_card_long_press_cb, LV_EVENT_LONG_PRESSED,
                                (void *)(intptr_t)i);
        }
    }

    s_ui.toast = mk_ui_label(lv_layer_top(), &mk_font_text_20, MK_COLOR_TEXT, "");
    lv_obj_set_style_bg_color(s_ui.toast, MK_COLOR_SURFACE_ALT, 0);
    lv_obj_set_style_bg_opa(s_ui.toast, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_ui.toast, 12, 0);
    lv_obj_set_style_radius(s_ui.toast, 8, 0);
    lv_obj_align(s_ui.toast, LV_ALIGN_TOP_MID, 0, MK_UI_CARD_Y - 60);
    lv_obj_add_flag(s_ui.toast, LV_OBJ_FLAG_HIDDEN);
}

/* Sets text and colour only on change: every style call invalidates the label. */
static void mk_ui_set_number(lv_obj_t *label, char *last, const char *text, int *last_color, bool live)
{
    if (strcmp(text, last) != 0) {
        strcpy(last, text);
        lv_label_set_text(label, text);
    }
    const int color = live ? 1 : 0;
    if (color != *last_color) {
        *last_color = color;
        lv_obj_set_style_text_color(label, live ? MK_COLOR_TEXT : MK_COLOR_TEXT_MUTED, 0);
    }
}

static void mk_ui_set_caption(mk_caption_t caption)
{
    if (caption == s_ui.caption) {
        return;
    }
    s_ui.caption = caption;
    lv_obj_t *cap = s_ui.rpm_caption;
    lv_label_set_text(cap, caption == CAPTION_SHIFT ? "TROQUE" : (caption == CAPTION_NO_SIGNAL ? "SEM SINAL" : "RPM"));
    lv_obj_set_style_bg_opa(cap, caption == CAPTION_SHIFT ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(cap, caption == CAPTION_SHIFT ? MK_COLOR_TEXT
                                : (caption == CAPTION_NO_SIGNAL ? MK_COLOR_TEXT_MUTED : MK_COLOR_BRAND), 0);
    lv_obj_set_width(cap, caption == CAPTION_SHIFT ? LV_SIZE_CONTENT : MK_UI_NUM_W);
    if (caption == CAPTION_SHIFT) {
        /* Content-sized pill, centred under the number. */
        lv_obj_update_layout(cap);
        lv_obj_set_x(cap, MK_UI_MARGIN + (MK_UI_NUM_W - lv_obj_get_width(cap)) / 2);
    } else {
        lv_obj_set_x(cap, MK_UI_MARGIN);
    }
}

static void mk_ui_refresh_cb(lv_timer_t *timer)
{
    (void)timer;
    mk_telemetry_snapshot_t snap;
    const uint32_t now = mk_ui_now_ms();
    mk_telemetry_snapshot(&snap, now);

    const mk_signal_sample_t *rpm = &snap.signals[MK_SIG_RPM];
    const mk_signal_sample_t *speed = &snap.signals[MK_SIG_SPEED];
    const bool shift = mk_rpm_widget_update(s_ui.rpm, rpm, now);
    const bool rpm_shown = rpm->quality == MK_SIGNAL_VALID || rpm->quality == MK_SIGNAL_STALE;
    const bool speed_shown = speed->quality == MK_SIGNAL_VALID || speed->quality == MK_SIGNAL_STALE;

    char text[12];
    /* RPM rounded to 10: the last digit is noise at ELM polling rates. */
    if (rpm_shown) {
        snprintf(text, sizeof(text), "%d", ((int)rpm->value + 5) / 10 * 10);
    } else {
        strcpy(text, "--");
    }
    mk_ui_set_number(s_ui.rpm_value, s_ui.rpm_text, text, &s_ui.rpm_color, rpm->quality == MK_SIGNAL_VALID);
    mk_ui_set_caption(shift ? CAPTION_SHIFT : (rpm->quality == MK_SIGNAL_VALID ? CAPTION_NORMAL : CAPTION_NO_SIGNAL));

    if (speed_shown) {
        snprintf(text, sizeof(text), "%d", (int)(speed->value + 0.5f));
    } else {
        strcpy(text, "--");
    }
    mk_ui_set_number(s_ui.speed_value, s_ui.speed_text, text, &s_ui.speed_color,
                     speed->quality == MK_SIGNAL_VALID);

    /* Gear: white digit when engaged; "-" for clutch/neutral or no data. */
    const mk_signal_sample_t *gear = &snap.signals[MK_SIG_GEAR];
    const bool gear_shown = (gear->quality == MK_SIGNAL_VALID || gear->quality == MK_SIGNAL_STALE)
                            && gear->value >= 1.0f;
    if (gear_shown) {
        snprintf(text, sizeof(text), "%d", (int)(gear->value + 0.5f));
    } else {
        strcpy(text, "-");
    }
    if (strcmp(text, s_ui.gear_text) != 0) {
        snprintf(s_ui.gear_text, sizeof(s_ui.gear_text), "%s", text);
        lv_label_set_text(s_ui.gear_value, text);
    }
    const int gear_color = gear_shown && gear->quality == MK_SIGNAL_VALID ? 1 : 0;
    if (gear_color != s_ui.gear_color) {
        s_ui.gear_color = gear_color;
        lv_obj_set_style_text_color(s_ui.gear_value, gear_color ? MK_COLOR_TEXT : MK_COLOR_TEXT_MUTED, 0);
    }
    /* Unknown speed counts as moving: never unlock settings on missing data. */
    s_ui.moving = !(speed->quality == MK_SIGNAL_VALID && speed->value <= MK_UI_LOCK_SPEED_KMH);

    for (int i = 0; i < MK_UI_CARD_COUNT; ++i) {
        if (s_ui.cards[i] != NULL) {
            mk_value_card_update(s_ui.cards[i], &snap.signals[mk_value_card_signal(s_ui.cards[i])]);
        }
    }
    if (s_ui.moving && mk_picker_is_open()) {
        mk_picker_close();
        mk_ui_show_toast("Configuração fechada: veículo em movimento");
    }
}

esp_err_t mk_ui_start(const mk_ui_config_t *config)
{
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "config");
    ESP_RETURN_ON_FALSE(config->rpm_max > 0 && config->rpm_warning < config->rpm_shift
                        && config->rpm_shift <= config->rpm_max, ESP_ERR_INVALID_ARG, TAG, "rpm limits");
    for (int i = 0; i < MK_UI_CARD_COUNT; ++i) {
        ESP_RETURN_ON_FALSE(mk_signal_info(config->cards[i]) != NULL, ESP_ERR_INVALID_ARG, TAG, "card %d", i);
    }
    memset(&s_ui, 0, sizeof(s_ui));
    s_ui.cfg = *config;
    s_ui.moving = true;
    s_ui.rpm_color = 1;
    s_ui.speed_color = 1;
    s_ui.gear_color = 0;
    s_ui.caption = CAPTION_NORMAL;

    lv_obj_t *screen = lv_screen_active();
    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, MK_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    mk_ui_build(screen);
    ESP_RETURN_ON_FALSE(s_ui.rpm != NULL, ESP_ERR_NO_MEM, TAG, "rpm widget");

    /* Splash: monogram over the dashboard, fading out after boot. */
    lv_obj_t *splash = lv_obj_create(lv_layer_top());
    mk_ui_style_plain(splash);
    lv_obj_set_size(splash, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(splash, MK_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(splash, LV_OPA_COVER, 0);
    lv_obj_t *logo = lv_image_create(splash);
    lv_image_set_src(logo, &mk_logo_splash);
    lv_obj_center(logo);
    lv_obj_fade_out(splash, MK_UI_SPLASH_FADE_MS, MK_UI_SPLASH_MS);
    lv_obj_delete_delayed(splash, MK_UI_SPLASH_MS + MK_UI_SPLASH_FADE_MS);

    lv_timer_create(mk_ui_refresh_cb, MK_UI_REFRESH_MS, NULL);
    mk_ui_refresh_cb(NULL);
    return ESP_OK;
}
