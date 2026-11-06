#pragma once

#include "esp_err.h"
#include "mk_gear.h"

/* Starts the task that derives signals (currently the gear) from published
 * telemetry. Works with any rpm/speed source. */
esp_err_t mk_derived_start(const mk_gear_ratios_t *ratios);
