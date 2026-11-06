#pragma once

#include "esp_err.h"

/* Starts the synthetic driving-cycle publisher (CONFIG_MK_TELEMETRY_SIM). */
esp_err_t mk_telemetry_sim_start(void);
