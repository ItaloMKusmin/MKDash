#include "mk_telemetry.h"

#include <string.h>

#if defined(ESP_PLATFORM)
#include "freertos/FreeRTOS.h"
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
#define MK_TELEMETRY_LOCK()   taskENTER_CRITICAL(&s_lock)
#define MK_TELEMETRY_UNLOCK() taskEXIT_CRITICAL(&s_lock)
#else
/* Host tests are single-threaded. */
#define MK_TELEMETRY_LOCK()   ((void)0)
#define MK_TELEMETRY_UNLOCK() ((void)0)
#endif

/* Ageing limits follow ARCHITECTURE.md 10.4. Ranges cover the generic OBD-II
 * definitions and the Corsa 1.0 8V engine; refine with real captures. */
static const mk_signal_info_t s_catalog[MK_SIG_COUNT] = {
    [MK_SIG_RPM]             = {"rpm",       "Rotação",      "rpm", 0, 0.0f,   9000.0f,  500, 2000},
    [MK_SIG_SPEED]           = {"speed",     "Velocidade",   "km/h", 0, 0.0f,  255.0f,   500, 2000},
    [MK_SIG_COOLANT_TEMP]    = {"coolant",   "Temp. motor",  "°C",   0, -40.0f, 215.0f,   3000, 5000},
    [MK_SIG_INTAKE_TEMP]     = {"intake",    "Temp. ar",     "°C",   0, -40.0f, 215.0f,   10000, 15000},
    [MK_SIG_MAP]             = {"map",       "MAP",          "kPa", 0, 0.0f,   255.0f,   500, 2000},
    [MK_SIG_TPS]             = {"tps",       "Borboleta",    "%",   0, 0.0f,   100.0f,   500, 2000},
    [MK_SIG_BATTERY_VOLTAGE] = {"battery",   "Bateria",      "V",   1, 0.0f,   20.0f,    3000, 5000},
    [MK_SIG_ENGINE_LOAD]     = {"load",      "Carga",        "%",   0, 0.0f,   100.0f,   1000, 3000},
    [MK_SIG_SPARK_ADVANCE]   = {"advance",   "Avanço",       "°", 1, -64.0f, 64.0f,  1000, 3000},
    [MK_SIG_INJECTOR_PULSE]  = {"injector",  "Injeção",      "ms",  2, 0.0f,   50.0f,    1000, 3000},
    [MK_SIG_O2_VOLTAGE]      = {"o2",        "Sonda lambda", "V",   2, 0.0f,   1.275f,   1000, 3000},
    [MK_SIG_IDLE_VALVE]      = {"iac",       "Marcha lenta", "passos", 0, 0.0f, 255.0f, 3000, 5000},
    [MK_SIG_FUEL_LEVEL]      = {"fuel",      "Combustível",  "%",   0, 0.0f,   100.0f,   10000, 15000},
    [MK_SIG_ENGINE_RUNTIME]  = {"runtime",   "Motor ligado", "s",   0, 0.0f,   65535.0f, 10000, 15000},
    [MK_SIG_OIL_TEMP]        = {"oil_temp",  "Temp. óleo",   "°C",  0, -40.0f, 215.0f,   3000, 5000},
    [MK_SIG_OIL_PRESSURE]    = {"oil_press", "Pressão óleo", "bar", 1, 0.0f,   10.0f,    1000, 3000},
    [MK_SIG_GEAR]            = {"gear",      "Marcha",       "",    0, 0.0f,   6.0f,     500, 2000},
};

static mk_signal_sample_t s_samples[MK_SIG_COUNT];

static bool mk_signal_id_valid(mk_signal_id_t id)
{
    return (unsigned)id < (unsigned)MK_SIG_COUNT;
}

const mk_signal_info_t *mk_signal_info(mk_signal_id_t id)
{
    return mk_signal_id_valid(id) ? &s_catalog[id] : NULL;
}

bool mk_signal_find(const char *key, mk_signal_id_t *out_id)
{
    if (key == NULL || out_id == NULL) {
        return false;
    }
    for (int i = 0; i < MK_SIG_COUNT; ++i) {
        if (strcmp(s_catalog[i].key, key) == 0) {
            *out_id = (mk_signal_id_t)i;
            return true;
        }
    }
    return false;
}

void mk_telemetry_reset(void)
{
    MK_TELEMETRY_LOCK();
    memset(s_samples, 0, sizeof(s_samples)); /* quality 0 == UNAVAILABLE */
    MK_TELEMETRY_UNLOCK();
}

void mk_telemetry_publish(mk_signal_id_t id, float value, uint32_t now_ms)
{
    if (!mk_signal_id_valid(id)) {
        return;
    }
    const mk_signal_info_t *info = &s_catalog[id];
    /* NaN fails both comparisons and is rejected as well. */
    const bool in_range = value >= info->min && value <= info->max;
    MK_TELEMETRY_LOCK();
    s_samples[id].value = value;
    s_samples[id].timestamp_ms = now_ms;
    s_samples[id].quality = in_range ? MK_SIGNAL_VALID : MK_SIGNAL_ERROR;
    MK_TELEMETRY_UNLOCK();
}

void mk_telemetry_mark_unavailable(mk_signal_id_t id)
{
    if (!mk_signal_id_valid(id)) {
        return;
    }
    MK_TELEMETRY_LOCK();
    s_samples[id].quality = MK_SIGNAL_UNAVAILABLE;
    MK_TELEMETRY_UNLOCK();
}

void mk_telemetry_snapshot(mk_telemetry_snapshot_t *out, uint32_t now_ms)
{
    if (out == NULL) {
        return;
    }
    MK_TELEMETRY_LOCK();
    memcpy(out->signals, s_samples, sizeof(s_samples));
    MK_TELEMETRY_UNLOCK();

    out->taken_ms = now_ms;
    for (int i = 0; i < MK_SIG_COUNT; ++i) {
        mk_signal_sample_t *sample = &out->signals[i];
        if (sample->quality == MK_SIGNAL_UNAVAILABLE) {
            continue;
        }
        /* Unsigned subtraction stays correct across the 49-day wrap. An old
         * ERROR also expires, so a lost link never leaves a frozen state. */
        const uint32_t age = now_ms - sample->timestamp_ms;
        if (age >= s_catalog[i].unavailable_ms) {
            sample->quality = MK_SIGNAL_UNAVAILABLE;
        } else if (sample->quality == MK_SIGNAL_VALID && age >= s_catalog[i].stale_ms) {
            sample->quality = MK_SIGNAL_STALE;
        }
    }
}
