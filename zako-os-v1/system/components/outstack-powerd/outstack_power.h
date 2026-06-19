/*
 * outstack_power.h — Outstack Power Governor for ZAKO OS
 *
 * Five-mode state machine with:
 *   - Battery threshold-driven transitions (with hysteresis)
 *   - Thermal override (temperature-driven Emergency entry)
 *   - Charging override (one mode better while fast-charging)
 *   - Process class gating (5 classes, bitmask per mode)
 *   - C0 signal broadcast on mode changes
 *   - Record emission (MODE_CHANGE, GATE, RESTORE via bus)
 *
 * Modes: FULL(0) → STD(1) → CONS(2) → CRIT(3) → EMRG(4)
 *
 * This file contains the core state machine logic.
 * Hardware interfaces (sysfs) are abstracted for testability.
 *
 * Copyright (c) 2026 Babb Works. Licensed under MIT.
 */

#ifndef OUTSTACK_POWER_H
#define OUTSTACK_POWER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * MODES
 * ======================================================================== */

#define OSP_MODE_FULL   0u   /* Full Power — all classes active */
#define OSP_MODE_STD    1u   /* Standard — OPPORTUNISTIC gated */
#define OSP_MODE_CONS   2u   /* Conservation — +BACKGROUND+DEFERRED gated */
#define OSP_MODE_CRIT   3u   /* Critical Reserve — powersave, 2 cores */
#define OSP_MODE_EMRG   4u   /* Emergency — only CRITICAL, 1 core */

#define OSP_MODE_COUNT  5u

/* ========================================================================
 * PROCESS CLASSES (bitmask)
 * ======================================================================== */

#define OSP_CLASS_OPPORTUNISTIC  0x01u  /* ML inference, speculative */
#define OSP_CLASS_DEFERRED       0x02u  /* Deferrable background work */
#define OSP_CLASS_BACKGROUND     0x04u  /* Sync, health data, PADS */
#define OSP_CLASS_INTERACTIVE    0x08u  /* UI, active apps */
#define OSP_CLASS_CRITICAL       0x10u  /* telux-ledgerd, identd, modem */

/* Gating masks per mode (which classes are GATED/frozen in each mode) */
#define OSP_GATE_FULL   0x00u  /* Nothing gated */
#define OSP_GATE_STD    0x01u  /* OPPORTUNISTIC */
#define OSP_GATE_CONS   0x07u  /* OPPORTUNISTIC+DEFERRED+BACKGROUND */
#define OSP_GATE_CRIT   0x07u  /* Same classes, but hard-gated + core park */
#define OSP_GATE_EMRG   0x0Fu  /* +INTERACTIVE (everything except CRITICAL) */

/* ========================================================================
 * THRESHOLDS
 * ======================================================================== */

#define OSP_THRESH_STANDARD     50u   /* Below this: FULL → STD */
#define OSP_THRESH_CONSERVE     20u   /* Below this: STD → CONS */
#define OSP_THRESH_LOW          10u   /* Below this: CONS → CRIT */
#define OSP_THRESH_SHUTDOWN      3u   /* Below this: CRIT → EMRG */
#define OSP_HYSTERESIS           3u   /* Band for upward transitions */

#define OSP_THERMAL_CRITICAL  45000   /* millidegrees C (45°C) */
#define OSP_THERMAL_SAFE      40000   /* millidegrees C (40°C) */
#define OSP_THERMAL_SUSTAIN_ENTER 60u /* seconds sustained above critical */
#define OSP_THERMAL_SUSTAIN_EXIT 120u /* seconds sustained below safe */

#define OSP_FAST_CHARGE_MW    10000u  /* 10W threshold for charge override */
#define OSP_CHARGE_FULL_BATT    30u   /* Above this + fast charge → FULL */

/* ========================================================================
 * TRANSITION FLAGS
 * ======================================================================== */

#define OSP_REASON_BATTERY   0x01u  /* Normal battery-driven */
#define OSP_REASON_THERMAL   0x02u  /* Thermal override */
#define OSP_REASON_CHARGING  0x04u  /* Charging override */

/* ========================================================================
 * ERROR CODES
 * ======================================================================== */

#define OSP_OK              0
#define OSP_ERR_NULL       (-1)
#define OSP_ERR_INVALID    (-2)
#define OSP_ERR_NO_CHANGE  (-3)  /* No transition needed */

/* ========================================================================
 * HARDWARE ABSTRACTION (for testability)
 *
 * On real hardware, these read sysfs. In tests, they're mocked.
 * ======================================================================== */

typedef struct {
    int      capacity_pct;      /* 0-100 */
    int      temp_mc;           /* millidegrees C (battery temp) */
    uint8_t  charging;          /* 1 if charging */
    uint32_t charge_rate_mw;    /* milliwatts charging rate */
} osp_battery_t;

typedef struct {
    int      temp_mc;           /* millidegrees C (hottest CPU zone) */
} osp_thermal_t;

/* ========================================================================
 * TRANSITION RECORD (emitted on mode change)
 * ======================================================================== */

typedef struct {
    uint8_t  prev_mode;
    uint8_t  new_mode;
    uint8_t  reason;            /* OSP_REASON_* flags */
    uint8_t  gate_mask;         /* Classes gated in new mode */
    int      battery_pct;       /* Battery at time of transition */
    int      temp_mc;           /* Temperature at time of transition */
} osp_transition_t;

/* ========================================================================
 * STATE MACHINE
 * ======================================================================== */

typedef struct {
    /* Current state */
    uint8_t  mode;              /* Current mode (0-4) */
    uint8_t  gate_mask;         /* Currently gated classes */
    uint8_t  thermal_override;  /* 1 if in thermal emergency */

    /* Battery state (last reading) */
    osp_battery_t battery;

    /* Thermal state */
    osp_thermal_t thermal;
    uint32_t thermal_sustain_s; /* Seconds sustained above/below threshold */
    uint8_t  thermal_above;     /* 1 if currently above THERMAL_CRITICAL */

    /* Hysteresis tracking */
    uint8_t  confirmed;         /* 1 if last threshold cross was confirmed */

    /* Stats */
    uint64_t transitions;
    uint64_t thermal_overrides;
    uint64_t charge_overrides;

    /* Transition log (last transition, for bus emission) */
    osp_transition_t last_transition;
    uint8_t          transition_pending; /* 1 if a new transition occurred */
} osp_state_t;

/* ========================================================================
 * PUBLIC API
 * ======================================================================== */

/*
 * osp_init — Initialize the state machine (starts in FULL mode).
 */
int osp_init(osp_state_t *state);

/*
 * osp_evaluate — Run one evaluation cycle.
 *
 * Given current battery and thermal readings, determines if a mode
 * transition is needed. If so, updates state and sets transition_pending.
 *
 * Call this once per sample interval (30s in FULL/STD/CONS, 30s in EMRG).
 *
 * @param state    The state machine
 * @param battery  Current battery readings
 * @param thermal  Current thermal readings
 * @param elapsed_s Seconds since last evaluation (for sustain timers)
 * @return OSP_OK if transition occurred, OSP_ERR_NO_CHANGE if no change
 */
int osp_evaluate(osp_state_t *state,
                 const osp_battery_t *battery,
                 const osp_thermal_t *thermal,
                 uint32_t elapsed_s);

/*
 * osp_get_gate_mask — Get the gating mask for a mode.
 */
uint8_t osp_get_gate_mask(uint8_t mode);

/*
 * osp_mode_name — Human-readable name for a mode.
 */
const char *osp_mode_name(uint8_t mode);

/*
 * osp_mode_for_battery — Determine mode from battery % alone (no hysteresis).
 *
 * Used for thermal recovery and charging override calculations.
 */
uint8_t osp_mode_for_battery(int capacity_pct);

/*
 * osp_consume_transition — Get and clear the pending transition.
 *
 * @param state  State machine
 * @param out    Output: transition record
 * @return OSP_OK if transition available, OSP_ERR_NO_CHANGE if none pending
 */
int osp_consume_transition(osp_state_t *state, osp_transition_t *out);

#ifdef __cplusplus
}
#endif

#endif /* OUTSTACK_POWER_H */
