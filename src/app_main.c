#include "mk_build_info.h"

#include "esp_log.h"

#ifdef MK_BUILD_TESTS
int mk_build_info_tests_run(void);
#endif

void app_main(void)
{
    const mk_build_info_t mk_build = mk_build_info_get();
    ESP_LOGI("mk_app", "MKAuto Dashboard %s", mk_build.version);
    ESP_LOGI("mk_app", "commit=%s dirty=%s idf=%s",
             mk_build.commit, mk_build.dirty, mk_build.idf_version);
    ESP_LOGI("mk_app", "Bootstrap ready; board peripherals are not initialized");
#ifdef MK_BUILD_TESTS
    const int mk_failures = mk_build_info_tests_run();
    ESP_LOGI("mk_app", "Bootstrap tests finished: failures=%d", mk_failures);
#endif
}
