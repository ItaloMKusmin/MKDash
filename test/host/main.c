#include "esp_app_desc.h"
#include "esp_idf_version.h"

const esp_app_desc_t *esp_app_get_description(void)
{
    static const esp_app_desc_t mk_description = {.version = "host-test-version"};
    return &mk_description;
}

const char *esp_get_idf_version(void)
{
    return "host-test-idf";
}

int mk_build_info_tests_run(void);

int main(void)
{
    return mk_build_info_tests_run();
}
