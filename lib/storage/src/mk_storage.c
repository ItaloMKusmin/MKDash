#include "mk_storage.h"

#include <stdbool.h>

#include "esp_check.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "mk_storage";
static const char *MK_STORAGE_NAMESPACE = "mkauto";
static nvs_handle_t s_handle;
static bool s_ready;

esp_err_t mk_storage_init(void)
{
    if (s_ready) {
        return ESP_OK;
    }
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS incompatible (%s), erasing", esp_err_to_name(err));
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "erase");
        err = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(err, TAG, "init");
    ESP_RETURN_ON_ERROR(nvs_open(MK_STORAGE_NAMESPACE, NVS_READWRITE, &s_handle), TAG, "open");
    s_ready = true;
    return ESP_OK;
}

esp_err_t mk_storage_get_string(const char *name, char *out, size_t out_len)
{
    ESP_RETURN_ON_FALSE(s_ready, ESP_ERR_INVALID_STATE, TAG, "not initialised");
    size_t len = out_len;
    return nvs_get_str(s_handle, name, out, &len);
}

esp_err_t mk_storage_set_string(const char *name, const char *value)
{
    ESP_RETURN_ON_FALSE(s_ready, ESP_ERR_INVALID_STATE, TAG, "not initialised");
    ESP_RETURN_ON_ERROR(nvs_set_str(s_handle, name, value), TAG, "set %s", name);
    return nvs_commit(s_handle);
}
