#include "mk_build_info.h"

#include "esp_app_desc.h"
#include "esp_idf_version.h"

mk_build_info_t mk_build_info_get(void)
{
    return (mk_build_info_t) {
        .version = esp_app_get_description()->version,
        .commit = MK_BUILD_COMMIT,
        .dirty = MK_BUILD_DIRTY,
        .idf_version = esp_get_idf_version(),
    };
}
