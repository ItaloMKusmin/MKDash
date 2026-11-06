#include "mk_gear.h"

#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

/* Same reference data as the simulator: 3.55/1.96/1.30/0.89/0.71, 4.87, 1.75 m. */
static mk_gear_ratios_t mk_test_ratios(void)
{
    static const float box[] = {3.55f, 1.96f, 1.30f, 0.89f, 0.71f};
    mk_gear_ratios_t r = {.count = 5};
    for (int i = 0; i < 5; ++i) {
        r.rpm_per_kmh[i] = mk_gear_ratio_from_box(box[i], 4.87f, 1.75f);
    }
    return r;
}

static uint8_t mk_settled(mk_gear_estimator_t *est, float rpm, float speed)
{
    mk_gear_update(est, rpm, speed, 1000);
    return mk_gear_update(est, rpm, speed, 1400);
}

static void test_ratio_from_box(void)
{
    /* 3.55 * 4.87 * 1000 / (60 * 1.75) = 164.65 rpm per km/h */
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 164.65f, mk_gear_ratio_from_box(3.55f, 4.87f, 1.75f));
}

static void test_each_gear_is_recognised(void)
{
    const mk_gear_ratios_t r = mk_test_ratios();
    for (int g = 0; g < 5; ++g) {
        mk_gear_estimator_t est;
        mk_gear_init(&est, &r);
        const float speed = 40.0f;
        TEST_ASSERT_EQUAL_UINT8(g + 1, mk_settled(&est, r.rpm_per_kmh[g] * speed, speed));
    }
}

static void test_small_deviation_still_matches(void)
{
    const mk_gear_ratios_t r = mk_test_ratios();
    mk_gear_estimator_t est;
    mk_gear_init(&est, &r);
    TEST_ASSERT_EQUAL_UINT8(3, mk_settled(&est, r.rpm_per_kmh[2] * 60.0f * 1.05f, 60.0f));
}

static void test_clutch_slip_gives_no_gear(void)
{
    const mk_gear_ratios_t r = mk_test_ratios();
    mk_gear_estimator_t est;
    mk_gear_init(&est, &r);
    TEST_ASSERT_EQUAL_UINT8(2, mk_settled(&est, r.rpm_per_kmh[1] * 50.0f, 50.0f));
    /* Clutch pressed: rpm drops to idle while the car still rolls. */
    TEST_ASSERT_EQUAL_UINT8(0, mk_gear_update(&est, 900.0f, 50.0f, 1500));
}

static void test_low_speed_gives_no_gear(void)
{
    const mk_gear_ratios_t r = mk_test_ratios();
    mk_gear_estimator_t est;
    mk_gear_init(&est, &r);
    TEST_ASSERT_EQUAL_UINT8(0, mk_settled(&est, r.rpm_per_kmh[0] * 5.0f, 5.0f));
    TEST_ASSERT_EQUAL_UINT8(0, mk_settled(&est, 850.0f, 0.0f));
}

static void test_new_gear_needs_to_settle(void)
{
    const mk_gear_ratios_t r = mk_test_ratios();
    mk_gear_estimator_t est;
    mk_gear_init(&est, &r);
    TEST_ASSERT_EQUAL_UINT8(0, mk_gear_update(&est, r.rpm_per_kmh[3] * 80.0f, 80.0f, 1000));
    TEST_ASSERT_EQUAL_UINT8(0, mk_gear_update(&est, r.rpm_per_kmh[3] * 80.0f, 80.0f, 1299));
    TEST_ASSERT_EQUAL_UINT8(4, mk_gear_update(&est, r.rpm_per_kmh[3] * 80.0f, 80.0f, 1300));
}

static void test_ratio_validation(void)
{
    mk_gear_ratios_t r = mk_test_ratios();
    TEST_ASSERT_TRUE(mk_gear_ratios_valid(&r));
    r.rpm_per_kmh[2] = r.rpm_per_kmh[1];
    TEST_ASSERT_FALSE(mk_gear_ratios_valid(&r));
    r = mk_test_ratios();
    r.count = 0;
    TEST_ASSERT_FALSE(mk_gear_ratios_valid(&r));
    r.count = MK_GEAR_MAX + 1;
    TEST_ASSERT_FALSE(mk_gear_ratios_valid(&r));
    TEST_ASSERT_FALSE(mk_gear_ratios_valid(NULL));
}

int mk_gear_tests_run(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ratio_from_box);
    RUN_TEST(test_each_gear_is_recognised);
    RUN_TEST(test_small_deviation_still_matches);
    RUN_TEST(test_clutch_slip_gives_no_gear);
    RUN_TEST(test_low_speed_gives_no_gear);
    RUN_TEST(test_new_gear_needs_to_settle);
    RUN_TEST(test_ratio_validation);
    return UNITY_END();
}
