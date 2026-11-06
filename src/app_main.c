#include <stdio.h>

#include "mk_bringup_screen.h"
#include "mk_bsp.h"
#include "mk_build_info.h"

#include "esp_err.h"
#include "esp_log.h"

#ifdef MK_BUILD_TESTS
int mk_build_info_tests_run(void);
#endif

static const char *TAG = "mk_app";

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

    lv_display_t *display = NULL;
    ESP_ERROR_CHECK(mk_bsp_display_start(&display));

    char version_line[96];
    snprintf(version_line, sizeof(version_line), "%s  commit %s  IDF %s",
             mk_build.version, mk_build.commit, mk_build.idf_version);
    if (mk_bsp_display_lock(1000)) {
        mk_bringup_screen_create(version_line, mk_bsp_touch_ok());
        mk_bsp_display_unlock();
    }
    ESP_ERROR_CHECK(mk_bsp_backlight_set(80));
    ESP_LOGI(TAG, "Bring-up screen ready (touch %s)", mk_bsp_touch_ok() ? "ok" : "unavailable");
}
