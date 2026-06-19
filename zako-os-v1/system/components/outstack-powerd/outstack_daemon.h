/*
 * outstack_daemon.h — outstack-powerd Daemon Shell
 *
 * Bus-connected event loop that:
 *   - Reads battery/thermal state (via abstracted HAL)
 *   - Evaluates the power state machine on each sample interval
 *   - Broadcasts C0 MODE_ENTER signals on transitions
 *   - Emits MODE_CHANGE and GATE records via the bus
 *   - Responds to COMMAND frames (force mode, lock, status query)
 *
 * Subscribes to bus category 14 (Control).
 *
 * The hardware abstraction layer (HAL) is a function pointer table,
 * allowing real sysfs on device and mock readings in tests.
 *
 * Copyright (c) 2026 Babb Works. Licensed under MIT.
 */

#ifndef OUTSTACK_DAEMON_H
#define OUTSTACK_DAEMON_H

#include <stdint.h>
#include "../libzako-bus/zako_bus.h"
#include "../libzako-c0/zako_c0.h"
#include "outstack_power.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * BUS PROTOCOL
 * ======================================================================== */

/* Bus category for power/control */
#define OSD_CAT_CONTROL      14u  /* ZBP_CAT_CONTROL = 0x0E */

/* Outbound frame types (emitted by powerd) */
#define OSD_FRAME_MODE_CHANGE  0x01u  /* [type][prev][new][reason][gate_mask] */
#define OSD_FRAME_GATE         0x02u  /* [type][gate_mask] */
#define OSD_FRAME_RESTORE      0x03u  /* [type][restore_mask] */
#define OSD_FRAME_C0_SIGNAL    0x04u  /* [type][c0_byte] */
#define OSD_FRAME_STATUS       0x05u  /* [type][mode][gate][batt%][temp_hi][temp_lo][flags] */

/* Inbound command opcodes */
#define OSD_CMD_STATUS         0x10u  /* Request status report */
#define OSD_CMD_FORCE_MODE     0x11u  /* [opcode][mode] — force mode (debug) */

/* ========================================================================
 * HARDWARE ABSTRACTION LAYER
 * ======================================================================== */

/*
 * Function pointers for hardware readings.
 * On real hardware: read sysfs.
 * In tests: return preset values.
 */
typedef struct {
    /* Read battery state. Returns 0 on success. */
    int (*read_battery)(osp_battery_t *out, void *ctx);
    /* Read thermal state. Returns 0 on success. */
    int (*read_thermal)(osp_thermal_t *out, void *ctx);
    /* Apply CPU governor/freq/cores for a mode. Returns 0 on success. */
    int (*apply_cpu)(uint8_t mode, void *ctx);
    /* Apply process gating. Returns 0 on success. */
    int (*apply_gate)(uint8_t gate_mask, void *ctx);
    /* User context pointer (passed to all callbacks) */
    void *ctx;
} osd_hal_t;

/* ========================================================================
 * ERROR CODES
 * ======================================================================== */

#define OSD_OK              0
#define OSD_ERR_NULL       (-1)
#define OSD_ERR_BUS        (-2)
#define OSD_ERR_HAL        (-3)
#define OSD_ERR_SHUTDOWN   (-4)

/* ========================================================================
 * DAEMON STATE
 * ======================================================================== */

#ifndef OSD_SAMPLE_INTERVAL_MS
#define OSD_SAMPLE_INTERVAL_MS  30000u  /* 30 seconds */
#endif

typedef struct {
    /* Bus */
    zbu_conn_t     bus;
    const char    *socket_path;

    /* State machine */
    osp_state_t    sm;

    /* HAL */
    osd_hal_t      hal;

    /* Runtime */
    uint8_t        running;
    uint32_t       sample_interval_ms;
    uint32_t       elapsed_since_last_s;  /* Seconds since last eval */

    /* Stats */
    uint64_t       samples_taken;
    uint64_t       c0_signals_sent;
    uint64_t       records_emitted;
} osd_daemon_t;

/* ========================================================================
 * PUBLIC API
 * ======================================================================== */

int osd_init(osd_daemon_t *daemon, const char *socket_path, const osd_hal_t *hal);
int osd_start(osd_daemon_t *daemon);

/*
 * osd_tick — Run one sample/evaluate cycle.
 *
 * Reads HAL, evaluates state machine, emits signals/records if transition.
 * Call this once per sample interval (or in tests, manually).
 *
 * @param daemon     Running daemon
 * @param elapsed_s  Seconds since last tick (for sustain timers)
 * @return OSD_OK, or error
 */
int osd_tick(osd_daemon_t *daemon, uint32_t elapsed_s);

/*
 * osd_poll — Check for inbound commands on the bus.
 *
 * Handles STATUS queries and FORCE_MODE commands.
 */
int osd_poll(osd_daemon_t *daemon);

void osd_stop(osd_daemon_t *daemon);
void osd_shutdown(osd_daemon_t *daemon);

#ifdef __cplusplus
}
#endif

#endif /* OUTSTACK_DAEMON_H */
