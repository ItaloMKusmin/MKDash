#pragma once

/*
 * Telemetry store: last value, timestamp and quality of each vehicle signal.
 * Sources (simulator today, ELM/Corsa protocol later) publish by signal id;
 * the UI reads coherent snapshots. Neither side knows about the other.
 */

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MK_SIG_RPM,
    MK_SIG_SPEED,
    MK_SIG_COOLANT_TEMP,
    MK_SIG_INTAKE_TEMP,
    MK_SIG_MAP,
    MK_SIG_TPS,
    MK_SIG_BATTERY_VOLTAGE,
    MK_SIG_ENGINE_LOAD,
    MK_SIG_SPARK_ADVANCE,
    MK_SIG_INJECTOR_PULSE,
    MK_SIG_O2_VOLTAGE,
    MK_SIG_IDLE_VALVE,
    MK_SIG_FUEL_LEVEL,
    MK_SIG_ENGINE_RUNTIME,
    MK_SIG_OIL_TEMP,        /* Needs a confirmed source; not universal (ARCHITECTURE.md 10.2). */
    MK_SIG_OIL_PRESSURE,
    MK_SIG_GEAR,            /* Derived from rpm/speed; 0 = no gear engaged or clutch slipping. */
    MK_SIG_COUNT
} mk_signal_id_t;

typedef enum {
    MK_SIGNAL_UNAVAILABLE,
    MK_SIGNAL_VALID,
    MK_SIGNAL_STALE,
    MK_SIGNAL_ERROR
} mk_signal_quality_t;

typedef struct {
    float value;
    uint32_t timestamp_ms;
    mk_signal_quality_t quality;
} mk_signal_sample_t;

typedef struct {
    const char *key;        /* Stable identifier for persisted settings; never rename. */
    const char *label;      /* Short user-facing name (pt-BR). */
    const char *unit;       /* Native unit, as stored. */
    uint8_t decimals;       /* Display precision. */
    float min;              /* Expected range, used for bars and validation. */
    float max;
    uint32_t stale_ms;      /* No update for this long: STALE. */
    uint32_t unavailable_ms; /* No update for this long: UNAVAILABLE. */
} mk_signal_info_t;

typedef struct {
    uint32_t taken_ms;
    mk_signal_sample_t signals[MK_SIG_COUNT];
} mk_telemetry_snapshot_t;

/* Catalog entry, or NULL for an invalid id. */
const mk_signal_info_t *mk_signal_info(mk_signal_id_t id);

/* Signal id for a catalog key; false if unknown. */
bool mk_signal_find(const char *key, mk_signal_id_t *out_id);

/* Clears every signal to UNAVAILABLE. */
void mk_telemetry_reset(void);

/* Stores a new value. Values outside [min, max] are stored as ERROR. */
void mk_telemetry_publish(mk_signal_id_t id, float value, uint32_t now_ms);

/* Source reports the signal as not supported by the vehicle. */
void mk_telemetry_mark_unavailable(mk_signal_id_t id);

/* Copies all signals, ageing VALID samples into STALE/UNAVAILABLE. */
void mk_telemetry_snapshot(mk_telemetry_snapshot_t *out, uint32_t now_ms);
