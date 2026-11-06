#pragma once

#include <stddef.h>

#include "esp_err.h"

/* Opens NVS, erasing it only when the partition layout is incompatible. */
esp_err_t mk_storage_init(void);

/* Reads a setting. ESP_ERR_NVS_NOT_FOUND when it was never written. */
esp_err_t mk_storage_get_string(const char *name, char *out, size_t out_len);

/* Writes and commits a setting. Not for high-frequency data. */
esp_err_t mk_storage_set_string(const char *name, const char *value);
