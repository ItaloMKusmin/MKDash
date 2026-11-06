#include "mk_telemetry.h"

#include <math.h>
#include <string.h>

#include "unity.h"

void setUp(void)
{
    mk_telemetry_reset();
}

void tearDown(void) {}

static mk_signal_sample_t mk_sample_at(mk_signal_id_t id, uint32_t now_ms)
{
    mk_telemetry_snapshot_t snap;
    mk_telemetry_snapshot(&snap, now_ms);
    return snap.signals[id];
}

static void test_never_published_is_unavailable(void)
{
    for (int i = 0; i < MK_SIG_COUNT; ++i) {
        TEST_ASSERT_EQUAL(MK_SIGNAL_UNAVAILABLE, mk_sample_at((mk_signal_id_t)i, 1000).quality);
    }
}

static void test_publish_is_valid_with_value_and_timestamp(void)
{
    mk_telemetry_publish(MK_SIG_RPM, 3250.0f, 1000);
    const mk_signal_sample_t s = mk_sample_at(MK_SIG_RPM, 1100);
    TEST_ASSERT_EQUAL(MK_SIGNAL_VALID, s.quality);
    TEST_ASSERT_EQUAL_FLOAT(3250.0f, s.value);
    TEST_ASSERT_EQUAL_UINT32(1000, s.timestamp_ms);
}

static void test_ageing_follows_catalog_limits(void)
{
    const mk_signal_info_t *rpm = mk_signal_info(MK_SIG_RPM);
    mk_telemetry_publish(MK_SIG_RPM, 900.0f, 1000);
    TEST_ASSERT_EQUAL(MK_SIGNAL_VALID, mk_sample_at(MK_SIG_RPM, 1000 + rpm->stale_ms - 1).quality);
    TEST_ASSERT_EQUAL(MK_SIGNAL_STALE, mk_sample_at(MK_SIG_RPM, 1000 + rpm->stale_ms).quality);
    TEST_ASSERT_EQUAL(MK_SIGNAL_STALE, mk_sample_at(MK_SIG_RPM, 1000 + rpm->unavailable_ms - 1).quality);
    TEST_ASSERT_EQUAL(MK_SIGNAL_UNAVAILABLE, mk_sample_at(MK_SIG_RPM, 1000 + rpm->unavailable_ms).quality);
}

static void test_new_publish_recovers_from_stale(void)
{
    mk_telemetry_publish(MK_SIG_SPEED, 40.0f, 1000);
    TEST_ASSERT_EQUAL(MK_SIGNAL_UNAVAILABLE, mk_sample_at(MK_SIG_SPEED, 9000).quality);
    mk_telemetry_publish(MK_SIG_SPEED, 42.0f, 9000);
    TEST_ASSERT_EQUAL(MK_SIGNAL_VALID, mk_sample_at(MK_SIG_SPEED, 9100).quality);
}

static void test_ageing_survives_timer_wrap(void)
{
    const uint32_t before_wrap = UINT32_MAX - 100U;
    mk_telemetry_publish(MK_SIG_RPM, 900.0f, before_wrap);
    TEST_ASSERT_EQUAL(MK_SIGNAL_VALID, mk_sample_at(MK_SIG_RPM, 200U).quality);   /* 301 ms */
    TEST_ASSERT_EQUAL(MK_SIGNAL_STALE, mk_sample_at(MK_SIG_RPM, 500U).quality);   /* 601 ms */
}

static void test_out_of_range_and_nan_are_error(void)
{
    mk_telemetry_publish(MK_SIG_TPS, 140.0f, 1000);
    TEST_ASSERT_EQUAL(MK_SIGNAL_ERROR, mk_sample_at(MK_SIG_TPS, 1000).quality);
    mk_telemetry_publish(MK_SIG_TPS, NAN, 1000);
    TEST_ASSERT_EQUAL(MK_SIGNAL_ERROR, mk_sample_at(MK_SIG_TPS, 1000).quality);
    mk_telemetry_publish(MK_SIG_TPS, 100.0f, 1000);
    TEST_ASSERT_EQUAL(MK_SIGNAL_VALID, mk_sample_at(MK_SIG_TPS, 1000).quality);
}

static void test_error_expires_to_unavailable(void)
{
    const mk_signal_info_t *tps = mk_signal_info(MK_SIG_TPS);
    mk_telemetry_publish(MK_SIG_TPS, -5.0f, 1000);
    TEST_ASSERT_EQUAL(MK_SIGNAL_ERROR, mk_sample_at(MK_SIG_TPS, 1000 + tps->stale_ms).quality);
    TEST_ASSERT_EQUAL(MK_SIGNAL_UNAVAILABLE, mk_sample_at(MK_SIG_TPS, 1000 + tps->unavailable_ms).quality);
}

static void test_mark_unavailable(void)
{
    mk_telemetry_publish(MK_SIG_FUEL_LEVEL, 50.0f, 1000);
    mk_telemetry_mark_unavailable(MK_SIG_FUEL_LEVEL);
    TEST_ASSERT_EQUAL(MK_SIGNAL_UNAVAILABLE, mk_sample_at(MK_SIG_FUEL_LEVEL, 1000).quality);
}

static void test_invalid_ids_are_ignored(void)
{
    mk_telemetry_publish(MK_SIG_COUNT, 1.0f, 1000);
    mk_telemetry_mark_unavailable((mk_signal_id_t)-1);
    TEST_ASSERT_NULL(mk_signal_info(MK_SIG_COUNT));
    mk_telemetry_snapshot(NULL, 1000);
}

static void test_catalog_is_complete_and_keys_unique(void)
{
    for (int i = 0; i < MK_SIG_COUNT; ++i) {
        const mk_signal_info_t *info = mk_signal_info((mk_signal_id_t)i);
        TEST_ASSERT_NOT_NULL(info);
        TEST_ASSERT_NOT_NULL(info->key);
        TEST_ASSERT_NOT_NULL(info->label);
        TEST_ASSERT_NOT_NULL(info->unit);
        TEST_ASSERT_TRUE(info->min < info->max);
        TEST_ASSERT_TRUE(info->stale_ms < info->unavailable_ms);
        mk_signal_id_t found;
        TEST_ASSERT_TRUE(mk_signal_find(info->key, &found));
        TEST_ASSERT_EQUAL(i, found);
    }
    mk_signal_id_t unused;
    TEST_ASSERT_FALSE(mk_signal_find("nao-existe", &unused));
    TEST_ASSERT_FALSE(mk_signal_find(NULL, &unused));
}

int mk_telemetry_tests_run(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_never_published_is_unavailable);
    RUN_TEST(test_publish_is_valid_with_value_and_timestamp);
    RUN_TEST(test_ageing_follows_catalog_limits);
    RUN_TEST(test_new_publish_recovers_from_stale);
    RUN_TEST(test_ageing_survives_timer_wrap);
    RUN_TEST(test_out_of_range_and_nan_are_error);
    RUN_TEST(test_error_expires_to_unavailable);
    RUN_TEST(test_mark_unavailable);
    RUN_TEST(test_invalid_ids_are_ignored);
    RUN_TEST(test_catalog_is_complete_and_keys_unique);
    return UNITY_END();
}
