#pragma once

typedef struct {
    const char *version;
    const char *commit;
    const char *dirty;
    const char *idf_version;
} mk_build_info_t;

mk_build_info_t mk_build_info_get(void);
