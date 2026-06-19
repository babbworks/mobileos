/*
 * outstack_daemon.c — outstack-powerd Daemon Shell Implementation
 *
 * Event loop: HAL reading → state machine evaluation → bus emission.
 *
 * Copyright (c) 2026 Babb Works. Licensed under MIT.
 */

#include "outstack_daemon.h"
#include <string.h>

/* ========================================================================
 * INTERNAL — BUS EMISSION
 * ======================================================================== */

static int emit_mode_change(osd_daemon_t *daemon, const osp_transition_t *t)
{
    uint8_t frame[6];
    frame[0] = OSD_FRAME_MODE_CHANGE;
    frame[1] = t->prev_mode;
    frame[2] = t->new_mode;
    frame[3] = t->reason;
    frame[4] = t->gate_mask;
    frame[5] = (uint8_t)(t->battery_pct & 0xFF);

    int rc = zbu_client_send(&daemon->bus, frame, 6);
    if (rc == ZBU_OK) { daemon->records_emitted++; }
    return (rc == ZBU_OK) ? OSD_OK : OSD_ERR_BUS;
}

static int emit_gate(osd_daemon_t *daemon, uint8_t gate_mask)
{
    uint8_t frame[2];
    frame[0] = OSD_FRAME_GATE;
    frame[1] = gate_mask;

    int rc = zbu_client_send(&daemon->bus, frame, 2);
    if (rc == ZBU_OK) { daemon->records_emitted++; }
    return (rc == ZBU_OK) ? OSD_OK : OSD_ERR_BUS;
}

static int emit_c0_signal(osd_daemon_t *daemon, uint8_t new_mode)
{
    /*
     * C0 signal for mode enter:
     * DC1 (0x11) = device control 1 (start/resume)
     * With priority flag if mode >= CRIT
     * Category encoded in continuation + code selection:
     *   Mode maps to DC1-DC4 codes:
     *     FULL=DC1, STD=DC2, CONS=DC3, CRIT=DC4, EMRG=DC4+Priority
     */
    uint8_t code;
    uint8_t priority = 0;

    switch (new_mode) {
    case OSP_MODE_FULL: code = ZC0_DC1; break;
    case OSP_MODE_STD:  code = ZC0_DC2; break;
    case OSP_MODE_CONS: code = ZC0_DC3; break;
    case OSP_MODE_CRIT: code = ZC0_DC4; break;
    case OSP_MODE_EMRG: code = ZC0_DC4; priority = 1; break;
    default: code = ZC0_DC1; break;
    }

    uint8_t c0_byte = zc0_encode(code, priority, 0, 0);

    uint8_t frame[2];
    frame[0] = OSD_FRAME_C0_SIGNAL;
    frame[1] = c0_byte;

    int rc = zbu_client_send(&daemon->bus, frame, 2);
    if (rc == ZBU_OK) { daemon->c0_signals_sent++; }
    return (rc == ZBU_OK) ? OSD_OK : OSD_ERR_BUS;
}

static int emit_status(osd_daemon_t *daemon)
{
    uint8_t frame[7];
    frame[0] = OSD_FRAME_STATUS;
    frame[1] = daemon->sm.mode;
    frame[2] = daemon->sm.gate_mask;
    frame[3] = (uint8_t)(daemon->sm.battery.capacity_pct & 0xFF);
    frame[4] = (uint8_t)((daemon->sm.thermal.temp_mc >> 8) & 0xFF);
    frame[5] = (uint8_t)(daemon->sm.thermal.temp_mc & 0xFF);
    frame[6] = (uint8_t)((daemon->sm.thermal_override ? 0x01 : 0x00) |
                          (daemon->sm.battery.charging ? 0x02 : 0x00));

    int rc = zbu_client_send(&daemon->bus, frame, 7);
    return (rc == ZBU_OK) ? OSD_OK : OSD_ERR_BUS;
}

/* ========================================================================
 * INTERNAL — COMMAND HANDLING
 * ======================================================================== */

static int handle_command(osd_daemon_t *daemon, const uint8_t *data, size_t len)
{
    if (len == 0) { return OSD_ERR_NULL; }

    switch (data[0]) {
    case OSD_CMD_STATUS:
        return emit_status(daemon);

    case OSD_CMD_FORCE_MODE:
        if (len >= 2 && data[1] < OSP_MODE_COUNT) {
            /* Force mode (debug only) — bypass state machine */
            uint8_t old = daemon->sm.mode;
            daemon->sm.mode = data[1];
            daemon->sm.gate_mask = osp_get_gate_mask(data[1]);

            /* Apply HAL */
            if (daemon->hal.apply_cpu) {
                daemon->hal.apply_cpu(data[1], daemon->hal.ctx);
            }
            if (daemon->hal.apply_gate) {
                daemon->hal.apply_gate(daemon->sm.gate_mask, daemon->hal.ctx);
            }

            /* Emit change */
            osp_transition_t t;
            t.prev_mode = old;
            t.new_mode = data[1];
            t.reason = 0; /* forced */
            t.gate_mask = daemon->sm.gate_mask;
            t.battery_pct = daemon->sm.battery.capacity_pct;
            t.temp_mc = daemon->sm.thermal.temp_mc;
            emit_c0_signal(daemon, data[1]);
            emit_mode_change(daemon, &t);
        }
        return OSD_OK;

    default:
        return OSD_OK;
    }
}

/* ========================================================================
 * PUBLIC API
 * ======================================================================== */

int osd_init(osd_daemon_t *daemon, const char *socket_path, const osd_hal_t *hal)
{
    if (daemon == NULL || socket_path == NULL || hal == NULL) {
        return OSD_ERR_NULL;
    }

    memset(daemon, 0, sizeof(*daemon));
    daemon->socket_path = socket_path;
    daemon->hal = *hal;
    daemon->running = 0;
    daemon->sample_interval_ms = OSD_SAMPLE_INTERVAL_MS;

    osp_init(&daemon->sm);
    return OSD_OK;
}

int osd_start(osd_daemon_t *daemon)
{
    int rc;

    if (daemon == NULL) { return OSD_ERR_NULL; }

    rc = zbu_client_init(&daemon->bus);
    if (rc != ZBU_OK) { return OSD_ERR_BUS; }

    rc = zbu_client_connect(&daemon->bus, daemon->socket_path);
    if (rc != ZBU_OK) { return OSD_ERR_BUS; }

    rc = zbu_client_subscribe(&daemon->bus, OSD_CAT_CONTROL);
    if (rc != ZBU_OK) {
        zbu_client_disconnect(&daemon->bus);
        return OSD_ERR_BUS;
    }

    daemon->running = 1;
    return OSD_OK;
}

int osd_tick(osd_daemon_t *daemon, uint32_t elapsed_s)
{
    osp_battery_t battery;
    osp_thermal_t thermal;
    osp_transition_t trans;
    int rc;

    if (daemon == NULL) { return OSD_ERR_NULL; }
    if (!daemon->running) { return OSD_ERR_SHUTDOWN; }

    /* Read HAL */
    if (daemon->hal.read_battery == NULL || daemon->hal.read_thermal == NULL) {
        return OSD_ERR_HAL;
    }

    rc = daemon->hal.read_battery(&battery, daemon->hal.ctx);
    if (rc != 0) { return OSD_ERR_HAL; }

    rc = daemon->hal.read_thermal(&thermal, daemon->hal.ctx);
    if (rc != 0) { return OSD_ERR_HAL; }

    daemon->samples_taken++;

    /* Evaluate state machine */
    rc = osp_evaluate(&daemon->sm, &battery, &thermal, elapsed_s);

    /* If transition occurred, emit signals and apply HAL */
    if (rc == OSP_OK) {
        rc = osp_consume_transition(&daemon->sm, &trans);
        if (rc == OSP_OK) {
            /* 1. C0 signal */
            emit_c0_signal(daemon, trans.new_mode);

            /* 2. MODE_CHANGE record */
            emit_mode_change(daemon, &trans);

            /* 3. GATE record */
            emit_gate(daemon, trans.gate_mask);

            /* 4. Apply hardware changes */
            if (daemon->hal.apply_cpu) {
                daemon->hal.apply_cpu(trans.new_mode, daemon->hal.ctx);
            }
            if (daemon->hal.apply_gate) {
                daemon->hal.apply_gate(trans.gate_mask, daemon->hal.ctx);
            }
        }
    }

    return OSD_OK;
}

int osd_poll(osd_daemon_t *daemon)
{
    zbu_frame_t frame;
    int rc;

    if (daemon == NULL) { return OSD_ERR_NULL; }
    if (!daemon->running) { return OSD_ERR_SHUTDOWN; }

    rc = zbu_client_recv(&daemon->bus, &frame);
    if (rc == ZBU_ERR_AGAIN) { return OSD_OK; }
    if (rc == ZBU_ERR_CLOSED) {
        daemon->running = 0;
        return OSD_ERR_BUS;
    }
    if (rc != ZBU_OK) { return OSD_ERR_BUS; }

    return handle_command(daemon, frame.data, frame.len);
}

void osd_stop(osd_daemon_t *daemon)
{
    if (daemon == NULL) { return; }
    daemon->running = 0;
}

void osd_shutdown(osd_daemon_t *daemon)
{
    if (daemon == NULL) { return; }
    daemon->running = 0;
    zbu_client_disconnect(&daemon->bus);
}
