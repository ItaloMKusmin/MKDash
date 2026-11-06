#include "mk_derived.h"

#include "esp_check.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mk_telemetry.h"

static const char *TAG = "mk_derived";

#define MK_DERIVED_PERIOD_MS 50

static mk_gear_estimator_t s_gear;

static void mk_derived_task(void *arg)
{
    (void)arg;
    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(MK_DERIVED_PERIOD_MS));
        const uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        mk_telemetry_snapshot_t snap;
        mk_telemetry_snapshot(&snap, now);
        const mk_signal_sample_t *rpm = &snap.signals[MK_SIG_RPM];
        const mk_signal_sample_t *speed = &snap.signals[MK_SIG_SPEED];
        if (rpm->quality != MK_SIGNAL_VALID || speed->quality != MK_SIGNAL_VALID) {
            /* No publish: the gear ages to STALE/UNAVAILABLE with its inputs. */
            mk_gear_reset(&s_gear);
            continue;
        }
        const uint8_t gear = mk_gear_update(&s_gear, rpm->value, speed->value, now);
        mk_telemetry_publish(MK_SIG_GEAR, (float)gear, now);
    }
}

esp_err_t mk_derived_start(const mk_gear_ratios_t *ratios)
{
    ESP_RETURN_ON_FALSE(mk_gear_ratios_valid(ratios), ESP_ERR_INVALID_ARG, TAG, "gear ratios");
    mk_gear_init(&s_gear, ratios);
    const BaseType_t ok = xTaskCreate(mk_derived_task, "mk_derived", 3072, NULL, 4, NULL);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
