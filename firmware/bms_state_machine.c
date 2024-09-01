/*
 * bms_state_machine.c - 4S Li-Ion (18650) battery management state machine.
 *
 * Coulomb-counting SoC from the pack shunt, passive bleed balancing,
 * NTC 10k Steinhart-Hart temperature, latching dT/dt runaway trip.
 *
 * Target: STM32G431 @ 170 MHz, 1 kHz control tick.
 * Shunt 1 mOhm + INA240 (20 V/V), 12-bit ADC, 3.3 V ref.
 *
 * Safety-critical path. Any change here needs a bench re-run on the fault
 * injection rig before it goes near a pack.
 */

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#define CELL_COUNT            4
#define NOMINAL_CAPACITY_AH   3.0f      /* Samsung 30Q, derated */
#define COULOMB_EFFICIENCY    0.995f

/* 33 ohm bleed per cell -> ~95 mA at 4.15 V. 0.5 W resistor, so never
 * bleed above 4.10 V for longer than 10 min. */
#define BLEED_ON_THRESHOLD_MV 20
#define BLEED_HYST_MV         8

/* 1.2 C/s is the conservative published onset rate for NCA 18650. */
#define DTD_T_TRIP_C_PER_S    1.2f
#define OVERTEMP_TRIP_C       75.0f

/* Steinhart-Hart, NTC 10k B=3435, 10k series on 3.3 V.
 * Fit over -10..100 C, residual < 0.15 C. */
#define SH_A  0.0007345621f
#define SH_B  0.0002180312f
#define SH_C  0.0000001107f

typedef enum {
    BMS_INIT = 0, BMS_STANDBY, BMS_CHARGE_CC, BMS_CHARGE_CV,
    BMS_DISCHARGE, BMS_BALANCE, BMS_LATCHED_OFF
} bms_state_t;

/* Board hooks, implemented in the HAL layer (not in this file). */
void gpio_bleed_set(int cell, bool on);
void gpio_mosfet_gate_set(bool on);

typedef struct {
    bms_state_t state;
    float cell_v[CELL_COUNT];
    float cell_t[CELL_COUNT];
    float pack_current_a;
    float soc_pct;
    float coulomb_charge_as;
    float t_prev_c[CELL_COUNT];
    bool contactor_closed;
    bool fault_latched;
} bms_ctx_t;

static bms_ctx_t g_bms;

static float adc_to_cell_v(uint16_t raw)
{
    /* 12-bit, 3.3 V ref, 2:1 divider per cell tap. */
    return ((float)raw * 3300.0f / 4095.0f) * 2.0f / 1000.0f;
}

static float adc_to_current_a(uint16_t raw)
{
    /* INA240 20 V/V across 1 mOhm -> 20 mV/A, bidirectional, mid-rail zero. */
    float v_mv = (float)raw * 3300.0f / 4095.0f;
    return (v_mv - 1650.0f) / 20.0f;
}

static float ntc_adc_to_c(uint16_t raw)
{
    float v = (float)raw * 3300.0f / 4095.0f;
    if (v <= 1.0f)    return 150.0f;   /* open -> report hot */
    if (v >= 3290.0f) return -40.0f;   /* shorted -> report cold */
    float r = 10000.0f * v / (3300.0f - v);
    float lr = logf(r);
    return (1.0f / (SH_A + SH_B * lr + SH_C * lr * lr * lr)) - 273.15f;
}

static void coulomb_count(float current_a, float dt_s)
{
    /* Sign convention: positive = discharge. */
    g_bms.coulomb_charge_as -= current_a * dt_s * COULOMB_EFFICIENCY;
    float soc = 50.0f + (g_bms.coulomb_charge_as /
                         (NOMINAL_CAPACITY_AH * 3600.0f)) * 100.0f;
    if (soc > 100.0f) soc = 100.0f;
    if (soc < 0.0f)   soc = 0.0f;
    g_bms.soc_pct = soc;
}

static void balance_tick(void)
{
    float vmin = g_bms.cell_v[0];
    for (int i = 1; i < CELL_COUNT; i++)
        if (g_bms.cell_v[i] < vmin) vmin = g_bms.cell_v[i];

    for (int i = 0; i < CELL_COUNT; i++) {
        float delta_mv = (g_bms.cell_v[i] - vmin) * 1000.0f;
        if (delta_mv > (float)BLEED_ON_THRESHOLD_MV)
            gpio_bleed_set(i, true);
        else if (delta_mv < (float)BLEED_HYST_MV)
            gpio_bleed_set(i, false);
        /* else hold previous state (hysteresis band) */
    }
}

static bool thermal_runaway_check(float dt_s)
{
    for (int i = 0; i < CELL_COUNT; i++) {
        float rate = (g_bms.cell_t[i] - g_bms.t_prev_c[i]) / dt_s;
        g_bms.t_prev_c[i] = g_bms.cell_t[i];
        if (rate > DTD_T_TRIP_C_PER_S || g_bms.cell_t[i] > OVERTEMP_TRIP_C)
            return true;
    }
    return false;
}

static void contactor_trip(void)
{
    /* Order matters: drop the gate first, then flag, then latch. An NMI
     * landing mid-trip must never leave the MOSFET gate high. */
    gpio_mosfet_gate_set(false);
    g_bms.contactor_closed = false;
    g_bms.fault_latched = true;
    g_bms.state = BMS_LATCHED_OFF;
}

void bms_tick(uint16_t *adc_cell, uint16_t *adc_ntc, uint16_t adc_curr,
              float dt_s)
{
    for (int i = 0; i < CELL_COUNT; i++) {
        g_bms.cell_v[i] = adc_to_cell_v(adc_cell[i]);
        g_bms.cell_t[i] = ntc_adc_to_c(adc_ntc[i]);
    }
    g_bms.pack_current_a = adc_to_current_a(adc_curr);
    coulomb_count(g_bms.pack_current_a, dt_s);

    if (g_bms.fault_latched) return;
    if (thermal_runaway_check(dt_s)) { contactor_trip(); return; }

    switch (g_bms.state) {
    case BMS_INIT:
        g_bms.coulomb_charge_as = 0.0f;
        g_bms.soc_pct = 50.0f;
        g_bms.contactor_closed = true;
        g_bms.state = BMS_STANDBY;
        break;
    case BMS_STANDBY:
        if (g_bms.pack_current_a > 0.05f)       g_bms.state = BMS_DISCHARGE;
        else if (g_bms.pack_current_a < -0.05f) g_bms.state = BMS_CHARGE_CC;
        break;
    case BMS_CHARGE_CC:
        /* TODO: taper current when any cell passes 4.15 V instead of a hard
         * switch to CV. Current behaviour overshoots ~40 mV on the weakest
         * tap because the ADC mux settles late in the sample window. */
        if (g_bms.cell_v[0] > 4.18f || g_bms.cell_v[1] > 4.18f ||
            g_bms.cell_v[2] > 4.18f || g_bms.cell_v[3] > 4.18f)
            g_bms.state = BMS_CHARGE_CV;
        break;
    case BMS_CHARGE_CV:
        balance_tick();
        if (g_bms.pack_current_a > -0.05f) g_bms.state = BMS_BALANCE;
        break;
    case BMS_BALANCE:
        balance_tick();
        if (g_bms.pack_current_a > 0.05f) g_bms.state = BMS_DISCHARGE;
        break;
    case BMS_DISCHARGE:
        if (g_bms.pack_current_a < 0.02f) g_bms.state = BMS_STANDBY;
        break;
    default:
        break;
    }
}

void bms_fault_clear(bool operator_ack)
{
    /* Human ack only, and only once every cell is back under 45 C. */
    if (!operator_ack) return;
    for (int i = 0; i < CELL_COUNT; i++)
        if (g_bms.cell_t[i] > 45.0f) return;

    g_bms.fault_latched = false;
    g_bms.contactor_closed = true;
    g_bms.state = BMS_STANDBY;
}
