#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "mk_telemetry.h"

#define MK_UI_CARD_COUNT 4

/* Called from the LVGL task when the user assigns a signal to a card. */
typedef void (*mk_ui_cards_changed_cb_t)(const mk_signal_id_t cards[MK_UI_CARD_COUNT]);

typedef struct {
    uint16_t rpm_max;           /* Full scale of the tachometer. */
    uint16_t rpm_warning;       /* Start of the attention zone. */
    uint16_t rpm_shift;         /* Shift alert threshold. */
    mk_signal_id_t cards[MK_UI_CARD_COUNT];
    mk_ui_cards_changed_cb_t on_cards_changed;
} mk_ui_config_t;

/* Builds the dashboard and starts refreshing it from telemetry snapshots.
 * Caller must hold the display lock. */
esp_err_t mk_ui_start(const mk_ui_config_t *config);
