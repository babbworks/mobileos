/*
 * outstack_power.c — Outstack Power State Machine Implementation
 *
 * Core logic: threshold evaluation, hysteresis, thermal override,
 * charging override, mode transitions.
 *
 * Copyright (c) 2026 Babb Works. Licensed under MIT.
 */

#include "outstack_power.h"
#include <string.h>

/* ========================================================================
 * GATE MASKS TABLE
 * ======================================================================== */

static const uint8_t GATE_TABLE[OSP_MODE_COUNT] = {
    OSP_GATE_FULL,  /* FULL: nothing gated */
    OSP_GATE_STD,   /* STD: OPPORTUNISTIC */
    OSP_GATE_CONS,  /* CONS: OPP+DEF+BG */
    OSP_GATE_CRIT,  /* CRIT: same as CONS (but hw settings differ) */
    OSP_GATE_EMRG   /* EMRG: everything except CRITICAL */
};

/* ========================================================================
 * INTERNAL HELPERS
 * ======================================================================== */

static void set_transition(osp_state_t *state, uint8_t new_mode, uint8_t reason)
{
    state->last_transition.prev_mode = state->mode;
    state->last_transition.new_mode = new_mode;
    state->last_transition.reason = reason;
    state->last_transition.gate_mask = GATE_TABLE[new_mode];
    state->last_transition.battery_pct = state->battery.capacity_pct;
    state->last_transition.temp_mc = state->thermal.temp_mc;

    state->mode = new_mode;
    state->gate_mask = GATE_TABLE[new_mode];
    state->transition_pending = 1;
    state->transitions++;
}

/*
 * Determine the raw battery-only mode (no hysteresis).
 */
static uint8_t raw_battery_mode(int pct)
{
    if (pct <= (int)OSP_THRESH_SHUTDOWN) { return OSP_MODE_EMRG; }
    if (pct <= (int)OSP_THRESH_LOW)      { return OSP_MODE_CRIT; }
    if (pct <= (int)OSP_THRESH_CONSERVE) { return OSP_MODE_CONS; }
    if (pct <= (int)OSP_THRESH_STANDARD) { return OSP_MODE_STD; }
    return OSP_MODE_FULL;
}

/*
 * Apply hysteresis for upward transitions.
 * Returns the threshold+hysteresis value for moving UP from current mode.
 */
static int upward_threshold(uint8_t current_mode)
{
    switch (current_mode) {
    case OSP_MODE_EMRG: return (int)(OSP_THRESH_SHUTDOWN + OSP_HYSTERESIS);
    case OSP_MODE_CRIT: return (int)(OSP_THRESH_LOW + OSP_HYSTERESIS);
    case OSP_MODE_CONS: return (int)(OSP_THRESH_CONSERVE + OSP_HYSTERESIS);
    case OSP_MODE_STD:  return (int)(OSP_THRESH_STANDARD + OSP_HYSTERESIS);
    default: return 100; /* FULL can't go up */
    }
}

/*
 * Determine the mode one step better (lower number = more power).
 */
static uint8_t one_mode_better(uint8_t mode)
{
    if (mode == OSP_MODE_FULL) { return OSP_MODE_FULL; }
    return mode - 1u;
}

/* ========================================================================
 * PUBLIC API
 * ======================================================================== */

int osp_init(osp_state_t *state)
{
    if (state == NULL) { return OSP_ERR_NULL; }

    memset(state, 0, sizeof(*state));
    state->mode = OSP_MODE_FULL;
    state->gate_mask = OSP_GATE_FULL;
    state->thermal_override = 0;
    state->thermal_sustain_s = 0;
    state->thermal_above = 0;
    state->confirmed = 0;
    state->transition_pending = 0;

    return OSP_OK;
}

int osp_evaluate(osp_state_t *state,
                 const osp_battery_t *battery,
                 const osp_thermal_t *thermal,
                 uint32_t elapsed_s)
{
    uint8_t target_mode;
    int pct;

    if (state == NULL || battery == NULL || thermal == NULL) {
        return OSP_ERR_NULL;
    }

    /* Update readings */
    state->battery = *battery;
    state->thermal = *thermal;

    pct = battery->capacity_pct;

    /* ================================================================
     * THERMAL OVERRIDE CHECK
     * ================================================================ */

    if (!state->thermal_override) {
        /* Not in thermal override — check for entry */
        if (thermal->temp_mc >= OSP_THERMAL_CRITICAL) {
            if (!state->thermal_above) {
                state->thermal_above = 1;
                state->thermal_sustain_s = 0;
            }
            state->thermal_sustain_s += elapsed_s;

            if (state->thermal_sustain_s >= OSP_THERMAL_SUSTAIN_ENTER) {
                /* THERMAL OVERRIDE → EMERGENCY */
                state->thermal_override = 1;
                state->thermal_overrides++;
                if (state->mode != OSP_MODE_EMRG) {
                    set_transition(state, OSP_MODE_EMRG, OSP_REASON_THERMAL);
                    return OSP_OK;
                }
            }
        } else {
            state->thermal_above = 0;
            state->thermal_sustain_s = 0;
        }
    } else {
        /* In thermal override — check for exit */
        if (thermal->temp_mc < OSP_THERMAL_SAFE) {
            state->thermal_sustain_s += elapsed_s;

            if (state->thermal_sustain_s >= OSP_THERMAL_SUSTAIN_EXIT) {
                /* THERMAL OVERRIDE RECOVERED */
                state->thermal_override = 0;
                state->thermal_above = 0;
                state->thermal_sustain_s = 0;

                /* Recovery mode = battery-determined mode at this moment */
                target_mode = raw_battery_mode(pct);
                if (target_mode != state->mode) {
                    set_transition(state, target_mode,
                                   OSP_REASON_BATTERY | OSP_REASON_THERMAL);
                    return OSP_OK;
                }
                return OSP_ERR_NO_CHANGE;
            }
        } else {
            /* Still hot — reset exit timer */
            state->thermal_sustain_s = 0;
        }
        /* While in thermal override, skip normal evaluation */
        return OSP_ERR_NO_CHANGE;
    }

    /* ================================================================
     * BATTERY-DRIVEN EVALUATION
     * ================================================================ */

    /* Downward transitions (battery dropping below thresholds) */
    target_mode = raw_battery_mode(pct);

    if (target_mode > state->mode) {
        /* Battery indicates we should be in a WORSE mode */
        set_transition(state, target_mode, OSP_REASON_BATTERY);
        /* Apply charging override after transition (below) */
    }
    /* Upward transitions (battery rising above threshold + hysteresis) */
    else if (target_mode < state->mode) {
        int up_thresh = upward_threshold(state->mode);
        if (pct > up_thresh) {
            /* Battery is above hysteresis threshold — move up one step */
            uint8_t new_mode = state->mode - 1u;
            set_transition(state, new_mode, OSP_REASON_BATTERY);
        }
    }

    /* ================================================================
     * CHARGING OVERRIDE
     *
     * "Operate one mode better than battery indicates."
     * Rule 1: rate > threshold → target = one_mode_better(battery_mode)
     * Rule 2: rate > threshold AND battery > 30% → FULL
     *
     * Override does NOT apply in Emergency mode.
     * Only triggers if current mode is worse than the override target.
     * ================================================================ */

    if (battery->charging && battery->charge_rate_mw >= OSP_FAST_CHARGE_MW) {
        /* Charging override does NOT apply in Emergency mode */
        if (state->mode == OSP_MODE_EMRG) {
            goto done;
        }

        uint8_t battery_mode = raw_battery_mode(pct);
        uint8_t override_target;

        /* Rule 2: Fast charge + battery > 30% → FULL */
        if (pct > (int)OSP_CHARGE_FULL_BATT) {
            override_target = OSP_MODE_FULL;
        } else {
            /* Rule 1: one mode better than what battery alone indicates.
             * Capped: Rule 1 cannot take you to FULL (only Rule 2 does). */
            override_target = one_mode_better(battery_mode);
            if (override_target == OSP_MODE_FULL) {
                override_target = OSP_MODE_STD; /* Cap at STD */
            }
        }

        /* Only apply if current mode is worse (higher number) than target */
        if (state->mode > override_target) {
            set_transition(state, override_target, OSP_REASON_CHARGING);
            state->charge_overrides++;
            return OSP_OK;
        }
    }

done:
    return state->transition_pending ? OSP_OK : OSP_ERR_NO_CHANGE;
}

uint8_t osp_get_gate_mask(uint8_t mode)
{
    if (mode >= OSP_MODE_COUNT) { return 0; }
    return GATE_TABLE[mode];
}

const char *osp_mode_name(uint8_t mode)
{
    static const char *names[OSP_MODE_COUNT] = {
        "FULL", "STANDARD", "CONSERVATION", "CRITICAL", "EMERGENCY"
    };
    if (mode >= OSP_MODE_COUNT) { return "UNKNOWN"; }
    return names[mode];
}

uint8_t osp_mode_for_battery(int capacity_pct)
{
    return raw_battery_mode(capacity_pct);
}

int osp_consume_transition(osp_state_t *state, osp_transition_t *out)
{
    if (state == NULL || out == NULL) { return OSP_ERR_NULL; }
    if (!state->transition_pending) { return OSP_ERR_NO_CHANGE; }

    *out = state->last_transition;
    state->transition_pending = 0;
    return OSP_OK;
}
