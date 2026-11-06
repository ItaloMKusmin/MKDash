#include <stdio.h>

#include "mk_bsp.h"
#include "mk_build_info.h"
#include "mk_derived.h"
#include "mk_storage.h"
#include "mk_telemetry.h"
#include "mk_telemetry_sim.h"
#include "mk_ui.h"
#include "sdkconfig.h"

#include "esp_err.h"
#include "esp_log.h"

#if CONFIG_MK_APP_BRINGUP_SCREEN
#include "mk_bringup_screen.h"
#endif

#ifdef MK_BUILD_TESTS
int mk_build_info_tests_run(void);
#endif

static const char *TAG = "mk_app";

/* Corsa Wind 1.0 8V baseline (DEC-011); becomes a vehicle profile setting. */
#define MK_APP_RPM_MAX     7000
#define MK_APP_RPM_WARNING 5000
#define MK_APP_RPM_SHIFT   5800

/* Provisional Corsa 1.0 8V gearbox (3.55/1.96/1.30/0.89/0.71, final 4.87,
 * 165/70 R13 = 1.75 m). To be replaced by the in-car learning procedure. */
static mk_gear_ratios_t mk_app_default_gears(void)
{
    static const float box[] = {3.55f, 1.96f, 1.30f, 0.89f, 0.71f};
    mk_gear_ratios_t r = {.count = 5};
    for (int i = 0; i < 5; ++i) {
        r.rpm_per_kmh[i] = mk_gear_ratio_from_box(box[i], 4.87f, 1.75f);
    }
    return r;
}

static const mk_signal_id_t s_default_cards[MK_UI_CARD_COUNT] = {
    MK_SIG_COOLANT_TEMP, MK_SIG_MAP, MK_SIG_BATTERY_VOLTAGE, MK_SIG_O2_VOLTAGE,
};

static void mk_app_card_setting(int index, char *name, size_t len)
{
    /* "l4" = 4-card layout; the earlier 8-card keys are intentionally ignored. */
    snprintf(name, len, "l4card%d", index);
}

/* Cards are stored by catalog key so reordering the enum keeps settings. */
static void mk_app_load_cards(mk_signal_id_t cards[MK_UI_CARD_COUNT])
{
    for (int i = 0; i < MK_UI_CARD_COUNT; ++i) {
        char name[12];
        char key[24];
        mk_signal_id_t id;
        cards[i] = s_default_cards[i];
        mk_app_card_setting(i, name, sizeof(name));
        if (mk_storage_get_string(name, key, sizeof(key)) == ESP_OK && mk_signal_find(key, &id)) {
            cards[i] = id;
        }
    }
}

static void mk_app_save_cards(const mk_signal_id_t cards[MK_UI_CARD_COUNT])
{
    for (int i = 0; i < MK_UI_CARD_COUNT; ++i) {
        char name[12];
        mk_app_card_setting(i, name, sizeof(name));
        const esp_err_t err = mk_storage_set_string(name, mk_signal_info(cards[i])->key);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "saving %s failed: %s", name, esp_err_to_name(err));
        }
    }
    ESP_LOGI(TAG, "card layout saved");
}

void app_main(void)
{
    const mk_build_info_t mk_build = mk_build_info_get();
    ESP_LOGI(TAG, "MKAuto Dashboard %s", mk_build.version);
    ESP_LOGI(TAG, "commit=%s dirty=%s idf=%s",
             mk_build.commit, mk_build.dirty, mk_build.idf_version);
#ifdef MK_BUILD_TESTS
    const int mk_failures = mk_build_info_tests_run();
    ESP_LOGI(TAG, "Bootstrap tests finished: failures=%d", mk_failures);
#endif

    const esp_err_t storage_err = mk_storage_init();
    if (storage_err != ESP_OK) {
        ESP_LOGE(TAG, "settings unavailable, using defaults: %s", esp_err_to_name(storage_err));
    }
    mk_telemetry_reset();

    lv_display_t *display = NULL;
    ESP_ERROR_CHECK(mk_bsp_display_start(&display));

#if CONFIG_MK_APP_BRINGUP_SCREEN
    char version_line[96];
    snprintf(version_line, sizeof(version_line), "%s  commit %s  IDF %s",
             mk_build.version, mk_build.commit, mk_build.idf_version);
    if (mk_bsp_display_lock(1000)) {
        mk_bringup_screen_create(version_line, mk_bsp_touch_ok());
        mk_bsp_display_unlock();
    }
#else
    mk_ui_config_t ui = {
        .rpm_max = MK_APP_RPM_MAX,
        .rpm_warning = MK_APP_RPM_WARNING,
        .rpm_shift = MK_APP_RPM_SHIFT,
        .on_cards_changed = mk_app_save_cards,
    };
    mk_app_load_cards(ui.cards);
    ESP_ERROR_CHECK(mk_bsp_display_lock(1000) ? ESP_OK : ESP_ERR_TIMEOUT);
    const esp_err_t ui_err = mk_ui_start(&ui);
    mk_bsp_display_unlock();
    ESP_ERROR_CHECK(ui_err);
#if CONFIG_MK_TELEMETRY_SIM
    ESP_ERROR_CHECK(mk_telemetry_sim_start());
#endif
    const mk_gear_ratios_t gears = mk_app_default_gears();
    ESP_ERROR_CHECK(mk_derived_start(&gears));
#endif
    ESP_ERROR_CHECK(mk_bsp_backlight_set(80));
    ESP_LOGI(TAG, "UI ready (touch %s)", mk_bsp_touch_ok() ? "ok" : "unavailable");
}
