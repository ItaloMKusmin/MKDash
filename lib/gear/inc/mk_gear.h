#pragma once

/*
 * Gear estimation for a manual gearbox: each gear has a fixed rpm/speed
 * ratio (gear x final drive / tyre), so the engaged gear is the calibrated
 * ratio closest to the measured one. Works with any speed source (ECU, GPS
 * or VSS). With the clutch pressed, in neutral or below the minimum speed
 * there is no match, and the result is "no gear" rather than a guess.
 */

#include <stdbool.h>
#include <stdint.h>

#define MK_GEAR_MAX 6

typedef struct {
    uint8_t count;                    /* number of forward gears */
    float rpm_per_kmh[MK_GEAR_MAX];   /* index 0 = 1st gear */
} mk_gear_ratios_t;

typedef struct {
    mk_gear_ratios_t ratios;
    uint8_t candidate;
    uint32_t candidate_since_ms;
    uint8_t current;                  /* 0 = no gear */
} mk_gear_estimator_t;

/* Ratio of one gear from gearbox data: rpm per km/h. */
float mk_gear_ratio_from_box(float gear_ratio, float final_drive, float tyre_circumference_m);

/* False when ratios are not strictly decreasing or count is out of range. */
bool mk_gear_ratios_valid(const mk_gear_ratios_t *ratios);

void mk_gear_init(mk_gear_estimator_t *est, const mk_gear_ratios_t *ratios);

/* Feeds one valid rpm/speed pair. Returns the gear (1..count) or 0. A new
 * gear is reported only after it has matched for MK_GEAR_SETTLE_MS. */
uint8_t mk_gear_update(mk_gear_estimator_t *est, float rpm, float speed_kmh, uint32_t now_ms);

/* Forgets the current gear (call when rpm or speed becomes unavailable). */
void mk_gear_reset(mk_gear_estimator_t *est);
