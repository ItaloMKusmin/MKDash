/*
 * Synthetic telemetry for UI development (ARCHITECTURE.md DEC-012).
 *
 * Repeats a driving cycle loosely modelled on a Corsa 1.0 8V: idle, full
 * acceleration through five gears (one pull close to the limiter), cruise,
 * coast down and idle again. Values are plausible, not a vehicle model.
 * Fuel level is reported as not supported, so the UI always shows one N/D.
 */
#include "mk_telemetry_sim.h"

#include <math.h>
#include <stdbool.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mk_telemetry.h"
#include "sdkconfig.h"

static const char *TAG = "mk_sim";

#define MK_SIM_PERIOD_MS     50
#define MK_SIM_IDLE_RPM      850.0f
#define MK_SIM_SHIFT_RPM     5600.0f
#define MK_SIM_LIMIT_PULL    6300.0f
#define MK_SIM_FINAL_DRIVE   4.87f
#define MK_SIM_WHEEL_M       1.75f   /* 165/70 R13 rolling circumference */
#define MK_SIM_DROPOUT_EVERY 90000U
#define MK_SIM_DROPOUT_LEN   3000U

static const float s_gear_ratio[] = {3.55f, 1.96f, 1.30f, 0.89f, 0.71f};
static const float s_rpm_rate[] = {2600.0f, 1500.0f, 950.0f, 600.0f, 380.0f}; /* rpm/s at WOT */
#define MK_SIM_GEARS ((int)(sizeof(s_gear_ratio) / sizeof(s_gear_ratio[0])))

typedef enum { PHASE_IDLE, PHASE_ACCEL, PHASE_CRUISE, PHASE_COAST } mk_sim_phase_t;

typedef struct {
    mk_sim_phase_t phase;
    uint32_t phase_ms;
    int gear;           /* 0-based, valid when moving */
    float rpm;
    float speed;
    float tps;
    float coolant;
    float runtime_s;
} mk_sim_state_t;

static float mk_sim_speed_for(float rpm, int gear)
{
    return rpm / (s_gear_ratio[gear] * MK_SIM_FINAL_DRIVE) * MK_SIM_WHEEL_M * 0.06f;
}

static float mk_sim_rpm_for(float speed, int gear)
{
    return speed / (MK_SIM_WHEEL_M * 0.06f) * s_gear_ratio[gear] * MK_SIM_FINAL_DRIVE;
}

static void mk_sim_enter(mk_sim_state_t *s, mk_sim_phase_t phase)
{
    s->phase = phase;
    s->phase_ms = 0;
}

static void mk_sim_step(mk_sim_state_t *s, float dt)
{
    s->phase_ms += (uint32_t)(dt * 1000.0f);
    s->runtime_s += dt;
    /* Warm-up from 25 C to a thermostat-controlled ~92 C. */
    s->coolant += (92.0f - s->coolant) * dt / 240.0f;

    switch (s->phase) {
    case PHASE_IDLE:
        s->tps = 0.0f;
        s->speed = 0.0f;
        s->rpm = MK_SIM_IDLE_RPM + 25.0f * sinf((float)s->phase_ms / 700.0f);
        if (s->phase_ms >= 12000U) {
            s->gear = 0;
            s->rpm = 1400.0f;
            mk_sim_enter(s, PHASE_ACCEL);
        }
        break;
    case PHASE_ACCEL: {
        s->tps = 85.0f;
        s->rpm += s_rpm_rate[s->gear] * dt;
        /* Third gear is pulled close to the limiter to exercise the red zone. */
        const float shift_at = (s->gear == 2) ? MK_SIM_LIMIT_PULL : MK_SIM_SHIFT_RPM;
        if (s->rpm >= shift_at) {
            if (s->gear + 1 < MK_SIM_GEARS) {
                s->rpm *= s_gear_ratio[s->gear + 1] / s_gear_ratio[s->gear];
                s->gear++;
            } else {
                mk_sim_enter(s, PHASE_CRUISE);
            }
        }
        if (s->gear == MK_SIM_GEARS - 1 && mk_sim_speed_for(s->rpm, s->gear) >= 110.0f) {
            mk_sim_enter(s, PHASE_CRUISE);
        }
        s->speed = mk_sim_speed_for(s->rpm, s->gear);
        break;
    }
    case PHASE_CRUISE:
        s->tps = 22.0f + 2.0f * sinf((float)s->phase_ms / 900.0f);
        s->speed += (108.0f - s->speed) * dt;
        s->rpm = mk_sim_rpm_for(s->speed, s->gear);
        if (s->phase_ms >= 10000U) {
            mk_sim_enter(s, PHASE_COAST);
        }
        break;
    case PHASE_COAST:
        s->tps = 0.0f;
        s->speed -= 6.0f * dt;
        while (s->gear > 0 && mk_sim_rpm_for(s->speed, s->gear) < 1500.0f) {
            s->gear--;
        }
        s->rpm = mk_sim_rpm_for(s->speed, s->gear);
        if (s->speed <= 8.0f || s->rpm < 1100.0f) {
            mk_sim_enter(s, PHASE_IDLE);
        }
        break;
    }
}

static void mk_sim_publish(const mk_sim_state_t *s, uint32_t now_ms)
{
    const float load = s->phase == PHASE_IDLE ? 18.0f : 15.0f + s->tps * 0.8f;
    const float map_kpa = 30.0f + s->tps * 0.68f;
    const float advance = s->phase == PHASE_IDLE ? 10.0f : 32.0f - s->tps * 0.18f;
    const float injector = 1.8f + load * 0.07f;
    const float o2 = 0.45f + 0.4f * sinf((float)now_ms / 350.0f);
    const float iac = s->phase == PHASE_IDLE ? 42.0f : 30.0f;
    const float battery = 13.9f + 0.2f * sinf((float)now_ms / 5000.0f);

    mk_telemetry_publish(MK_SIG_RPM, s->rpm, now_ms);
    mk_telemetry_publish(MK_SIG_SPEED, s->speed < 0.0f ? 0.0f : s->speed, now_ms);
    mk_telemetry_publish(MK_SIG_TPS, s->tps, now_ms);
    mk_telemetry_publish(MK_SIG_MAP, map_kpa, now_ms);
    mk_telemetry_publish(MK_SIG_ENGINE_LOAD, load, now_ms);
    mk_telemetry_publish(MK_SIG_SPARK_ADVANCE, advance, now_ms);
    mk_telemetry_publish(MK_SIG_INJECTOR_PULSE, injector, now_ms);
    mk_telemetry_publish(MK_SIG_O2_VOLTAGE, o2, now_ms);
    mk_telemetry_publish(MK_SIG_IDLE_VALVE, iac, now_ms);
    mk_telemetry_publish(MK_SIG_COOLANT_TEMP, s->coolant, now_ms);
    mk_telemetry_publish(MK_SIG_INTAKE_TEMP, 31.0f, now_ms);
    mk_telemetry_publish(MK_SIG_BATTERY_VOLTAGE, battery, now_ms);
    mk_telemetry_publish(MK_SIG_ENGINE_RUNTIME, floorf(s->runtime_s), now_ms);
    /* Oil follows coolant with lag; pressure rises with rpm and drops when hot. */
    const float oil_temp = 25.0f + (s->coolant - 25.0f) * 1.05f;
    const float oil_bar = 1.0f + s->rpm / 1500.0f - (oil_temp - 25.0f) * 0.008f;
    mk_telemetry_publish(MK_SIG_OIL_TEMP, oil_temp, now_ms);
    mk_telemetry_publish(MK_SIG_OIL_PRESSURE, oil_bar < 0.0f ? 0.0f : oil_bar, now_ms);
}

static uint32_t mk_sim_now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void mk_sim_task(void *arg)
{
    (void)arg;
    mk_sim_state_t state = {.phase = PHASE_IDLE, .rpm = MK_SIM_IDLE_RPM, .coolant = 25.0f};
    const float dt = (float)MK_SIM_PERIOD_MS / 1000.0f;
    TickType_t wake = xTaskGetTickCount();

    mk_telemetry_mark_unavailable(MK_SIG_FUEL_LEVEL);
    for (;;) {
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(MK_SIM_PERIOD_MS));
        const uint32_t now_ms = mk_sim_now_ms();
        mk_sim_step(&state, dt);
#if CONFIG_MK_TELEMETRY_SIM_DROPOUTS
        const bool dropout = (now_ms % MK_SIM_DROPOUT_EVERY) >= (MK_SIM_DROPOUT_EVERY - MK_SIM_DROPOUT_LEN);
        if (dropout) {
            continue;
        }
#endif
        mk_sim_publish(&state, now_ms);
    }
}

esp_err_t mk_telemetry_sim_start(void)
{
    ESP_LOGW(TAG, "SIMULATED telemetry active - values are not from a vehicle");
    const BaseType_t ok = xTaskCreate(mk_sim_task, "mk_sim", 4096, NULL, 4, NULL);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
