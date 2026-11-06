#pragma once

#include <stdbool.h>

/* Builds the M0 hardware test screen. Caller must hold the display lock. */
void mk_bringup_screen_create(const char *version_line, bool touch_ok);
