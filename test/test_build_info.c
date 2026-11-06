#include "mk_build_info.h"

#include "esp_app_desc.h"
#include "esp_idf_version.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void mk_test_build_versions(void)
{
    const mk_build_info_t mk_info = mk_build_info_get();
    TEST_ASSERT_NOT_NULL(mk_info.version);
    TEST_ASSERT_NOT_EQUAL('\0', mk_info.version[0]);
    TEST_ASSERT_EQUAL_STRING(esp_app_get_description()->version, mk_info.version);
    TEST_ASSERT_NOT_NULL(mk_info.idf_version);
    TEST_ASSERT_EQUAL_STRING(esp_get_idf_version(), mk_info.idf_version);
}

static void mk_test_build_git_metadata(void)
{
    const mk_build_info_t mk_info = mk_build_info_get();
    TEST_ASSERT_EQUAL_STRING(MK_BUILD_COMMIT, mk_info.commit);
    TEST_ASSERT_EQUAL_STRING(MK_BUILD_DIRTY, mk_info.dirty);
}

int mk_build_info_tests_run(void)
{
    UNITY_BEGIN();
    RUN_TEST(mk_test_build_versions);
    RUN_TEST(mk_test_build_git_metadata);
    return UNITY_END();
}
