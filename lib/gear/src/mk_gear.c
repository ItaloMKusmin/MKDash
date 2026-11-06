#include "mk_gear.h"

#include <math.h>
#include <string.h>

/* Below this speed the rpm/speed ratio is dominated by clutch slip. */
#define MK_GEAR_MIN_SPEED_KMH 8.0f
#define MK_GEAR_MIN_RPM       700.0f
/* Accepted deviation from a calibrated ratio. Adjacent gears of a typical
 * 5-speed differ by 20-45 %, so 8 % keeps them apart. */
#define MK_GEAR_TOLERANCE     0.08f
#define MK_GEAR_SETTLE_MS     300U

float mk_gear_ratio_from_box(float gear_ratio, float final_drive, float tyre_circumference_m)
{
    /* wheel rpm = speed[km/h] * 1000 / 60 / circumference */
    return gear_ratio * final_drive * 1000.0f / (60.0f * tyre_circumference_m);
}

bool mk_gear_ratios_valid(const mk_gear_ratios_t *ratios)
{
    if (ratios == NULL || ratios->count == 0 || ratios->count > MK_GEAR_MAX) {
        return false;
    }
    for (int i = 0; i < ratios->count; ++i) {
        if (!(ratios->rpm_per_kmh[i] > 0.0f)) {
            return false;
        }
        if (i > 0 && !(ratios->rpm_per_kmh[i] < ratios->rpm_per_kmh[i - 1])) {
            return false;
        }
    }
    return true;
}

void mk_gear_init(mk_gear_estimator_t *est, const mk_gear_ratios_t *ratios)
{
    memset(est, 0, sizeof(*est));
    est->ratios = *ratios;
}

void mk_gear_reset(mk_gear_estimator_t *est)
{
    est->candidate = 0;
    est->current = 0;
    est->candidate_since_ms = 0;
}

static uint8_t mk_gear_match(const mk_gear_ratios_t *ratios, float rpm, float speed_kmh)
{
    if (speed_kmh < MK_GEAR_MIN_SPEED_KMH || rpm < MK_GEAR_MIN_RPM) {
        return 0;
    }
    const float measured = rpm / speed_kmh;
    uint8_t best = 0;
    float best_error = MK_GEAR_TOLERANCE;
    for (int i = 0; i < ratios->count; ++i) {
        const float error = fabsf(measured - ratios->rpm_per_kmh[i]) / ratios->rpm_per_kmh[i];
        if (error <= best_error) {
            best_error = error;
            best = (uint8_t)(i + 1);
        }
    }
    return best;
}

uint8_t mk_gear_update(mk_gear_estimator_t *est, float rpm, float speed_kmh, uint32_t now_ms)
{
    const uint8_t match = mk_gear_match(&est->ratios, rpm, speed_kmh);
    if (match != est->candidate) {
        est->candidate = match;
        est->candidate_since_ms = now_ms;
    }
    /* "No gear" is reported at once (clutch pressed); a gear must settle. */
    if (match == 0) {
        est->current = 0;
    } else if (now_ms - est->candidate_since_ms >= MK_GEAR_SETTLE_MS) {
        est->current = match;
    }
    return est->current;
}
