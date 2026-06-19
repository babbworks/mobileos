/*
 * test_daemon.c — Unit tests for outstack-powerd daemon shell
 *
 * Tests bus integration, C0 signal emission, record emission,
 * HAL abstraction, and command handling via mock HAL + local bus.
 */

#include "outstack_daemon.h"
#include "../libzako-bus/zako_bus.h"
#include "../libzako-c0/zako_c0.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        fprintf(stderr, "  FAIL: %s (line %d)\n", (msg), __LINE__); \
    } else { \
        tests_passed++; \
    } \
} while (0)

#define TEST_SOCKET_PATH "/tmp/zako_outstack_test.sock"

/* ========================================================================
 * MOCK HAL
 * ======================================================================== */

typedef struct {
    osp_battery_t battery;
    osp_thermal_t thermal;
    uint8_t       last_cpu_mode;
    uint8_t       last_gate_mask;
    int           cpu_calls;
    int           gate_calls;
} mock_hal_ctx_t;

static int mock_read_battery(osp_battery_t *out, void *ctx)
{
    mock_hal_ctx_t *m = (mock_hal_ctx_t *)ctx;
    *out = m->battery;
    return 0;
}

static int mock_read_thermal(osp_thermal_t *out, void *ctx)
{
    mock_hal_ctx_t *m = (mock_hal_ctx_t *)ctx;
    *out = m->thermal;
    return 0;
}

static int mock_apply_cpu(uint8_t mode, void *ctx)
{
    mock_hal_ctx_t *m = (mock_hal_ctx_t *)ctx;
    m->last_cpu_mode = mode;
    m->cpu_calls++;
    return 0;
}

static int mock_apply_gate(uint8_t gate_mask, void *ctx)
{
    mock_hal_ctx_t *m = (mock_hal_ctx_t *)ctx;
    m->last_gate_mask = gate_mask;
    m->gate_calls++;
    return 0;
}

static void mock_init(mock_hal_ctx_t *m)
{
    memset(m, 0, sizeof(*m));
    m->battery.capacity_pct = 100;
    m->battery.temp_mc = 25000;
    m->battery.charging = 0;
    m->battery.charge_rate_mw = 0;
    m->thermal.temp_mc = 30000;
}

/* ========================================================================
 * TEST ENVIRONMENT
 * ======================================================================== */

typedef struct {
    zbu_server_t   server;
    osd_daemon_t   daemon;
    mock_hal_ctx_t mock;
} test_env_t;

static int env_setup(test_env_t *env)
{
    int rc;
    osd_hal_t hal;

    mock_init(&env->mock);

    hal.read_battery = mock_read_battery;
    hal.read_thermal = mock_read_thermal;
    hal.apply_cpu = mock_apply_cpu;
    hal.apply_gate = mock_apply_gate;
    hal.ctx = &env->mock;

    zbu_server_init(&env->server);
    rc = zbu_server_start(&env->server, TEST_SOCKET_PATH);
    if (rc != ZBU_OK) { return -1; }

    osd_init(&env->daemon, TEST_SOCKET_PATH, &hal);
    rc = osd_start(&env->daemon);
    if (rc != OSD_OK) {
        zbu_server_stop(&env->server);
        return -1;
    }

    usleep(10000);
    zbu_server_poll(&env->server, 100);

    /* Drain subscription frame */
    zbu_frame_t sub;
    usleep(10000);
    zbu_server_poll(&env->server, 100);
    while (zbu_server_recv(&env->server, &sub) == ZBU_OK) { }

    return 0;
}

static void env_teardown(test_env_t *env)
{
    osd_shutdown(&env->daemon);
    zbu_server_stop(&env->server);
    usleep(10000);
}

/* Receive all frames from daemon */
static int env_recv_frames(test_env_t *env, zbu_frame_t *frames, size_t max, size_t *count)
{
    size_t n = 0;
    usleep(10000);
    zbu_server_poll(&env->server, 100);
    while (n < max) {
        int rc = zbu_server_recv(&env->server, &frames[n]);
        if (rc != ZBU_OK) { break; }
        n++;
    }
    *count = n;
    return 0;
}

/* Send a command to daemon */
static int env_send_cmd(test_env_t *env, const uint8_t *data, size_t len)
{
    size_t i;
    for (i = 0; i < ZBU_MAX_CLIENTS; i++) {
        if (env->server.clients[i].state == ZBU_STATE_ACTIVE) {
            return zbu_server_send(&env->server, i, data, len);
        }
    }
    return -1;
}

/* ======================================================================== */

static void test_init_start_shutdown(void)
{
    osd_daemon_t d;
    mock_hal_ctx_t m;
    osd_hal_t hal;

    printf("test_init_start_shutdown:\n");

    mock_init(&m);
    hal.read_battery = mock_read_battery;
    hal.read_thermal = mock_read_thermal;
    hal.apply_cpu = mock_apply_cpu;
    hal.apply_gate = mock_apply_gate;
    hal.ctx = &m;

    ASSERT(osd_init(NULL, "/tmp/x", &hal) == OSD_ERR_NULL, "init NULL daemon");
    ASSERT(osd_init(&d, NULL, &hal) == OSD_ERR_NULL, "init NULL path");
    ASSERT(osd_init(&d, "/tmp/x", NULL) == OSD_ERR_NULL, "init NULL hal");

    int rc = osd_init(&d, TEST_SOCKET_PATH, &hal);
    ASSERT(rc == OSD_OK, "init OK");
    ASSERT(d.running == 0, "not running");
    ASSERT(d.sm.mode == OSP_MODE_FULL, "SM starts FULL");

    /* Start without server → fail */
    rc = osd_start(&d);
    ASSERT(rc == OSD_ERR_BUS, "start fails without server");
}

static void test_tick_no_transition(void)
{
    test_env_t env;
    zbu_frame_t frames[16];
    size_t count;

    printf("test_tick_no_transition:\n");

    if (env_setup(&env) != 0) { ASSERT(0, "setup"); return; }

    /* Battery at 100%, no transition expected */
    env.mock.battery.capacity_pct = 100;
    int rc = osd_tick(&env.daemon, 30);
    ASSERT(rc == OSD_OK, "tick OK");
    ASSERT(env.daemon.sm.mode == OSP_MODE_FULL, "stays FULL");
    ASSERT(env.daemon.samples_taken == 1, "1 sample");

    /* No frames emitted */
    env_recv_frames(&env, frames, 16, &count);
    ASSERT(count == 0, "no frames on no-transition");

    /* HAL not called for CPU/gate (no change) */
    ASSERT(env.mock.cpu_calls == 0, "no CPU change");
    ASSERT(env.mock.gate_calls == 0, "no gate change");

    env_teardown(&env);
}

static void test_tick_with_transition(void)
{
    test_env_t env;
    zbu_frame_t frames[16];
    size_t count;

    printf("test_tick_with_transition:\n");

    if (env_setup(&env) != 0) { ASSERT(0, "setup"); return; }

    /* Drop battery to 50% → FULL→STD */
    env.mock.battery.capacity_pct = 50;
    osd_tick(&env.daemon, 30);

    ASSERT(env.daemon.sm.mode == OSP_MODE_STD, "transition to STD");

    /* Should have emitted: C0_SIGNAL + MODE_CHANGE + GATE = 3 frames */
    env_recv_frames(&env, frames, 16, &count);
    ASSERT(count == 3, "3 frames emitted on transition");

    /* Frame 0: C0 signal */
    ASSERT(frames[0].data[0] == OSD_FRAME_C0_SIGNAL, "frame 0 is C0 signal");
    /* Decode C0 byte — should be DC2 (mode STD) */
    zc0_enhanced_t c0;
    zc0_decode(frames[0].data[1], &c0);
    ASSERT(c0.code == ZC0_DC2, "C0 code = DC2 (STD)");
    ASSERT(c0.priority == 0, "no priority flag for STD");

    /* Frame 1: MODE_CHANGE */
    ASSERT(frames[1].data[0] == OSD_FRAME_MODE_CHANGE, "frame 1 is MODE_CHANGE");
    ASSERT(frames[1].data[1] == OSP_MODE_FULL, "prev=FULL");
    ASSERT(frames[1].data[2] == OSP_MODE_STD, "new=STD");
    ASSERT(frames[1].data[3] == OSP_REASON_BATTERY, "reason=BATTERY");
    ASSERT(frames[1].data[4] == OSP_GATE_STD, "gate_mask=STD");

    /* Frame 2: GATE */
    ASSERT(frames[2].data[0] == OSD_FRAME_GATE, "frame 2 is GATE");
    ASSERT(frames[2].data[1] == OSP_GATE_STD, "gate mask = OPPORTUNISTIC");

    /* HAL was called */
    ASSERT(env.mock.cpu_calls == 1, "CPU mode applied");
    ASSERT(env.mock.last_cpu_mode == OSP_MODE_STD, "CPU set to STD");
    ASSERT(env.mock.gate_calls == 1, "gate applied");
    ASSERT(env.mock.last_gate_mask == OSP_GATE_STD, "gate = OPPORTUNISTIC");

    env_teardown(&env);
}

static void test_c0_signal_priority(void)
{
    test_env_t env;
    zbu_frame_t frames[16];
    size_t count;

    printf("test_c0_signal_priority:\n");

    if (env_setup(&env) != 0) { ASSERT(0, "setup"); return; }

    /* Drop to Emergency — C0 should have priority flag */
    env.mock.battery.capacity_pct = 2;
    osd_tick(&env.daemon, 30);

    env_recv_frames(&env, frames, 16, &count);
    ASSERT(count >= 1, "got frames");

    /* Find the C0 signal frame */
    zc0_enhanced_t c0;
    zc0_decode(frames[0].data[1], &c0);
    ASSERT(c0.code == ZC0_DC4, "EMRG uses DC4");
    ASSERT(c0.priority == 1, "EMRG has priority flag");

    env_teardown(&env);
}

static void test_multiple_transitions(void)
{
    test_env_t env;
    zbu_frame_t frames[32];
    size_t count;

    printf("test_multiple_transitions:\n");

    if (env_setup(&env) != 0) { ASSERT(0, "setup"); return; }

    /* Full discharge: 100 → 50 → 20 → 10 → 3 */
    env.mock.battery.capacity_pct = 50;
    osd_tick(&env.daemon, 30);
    env_recv_frames(&env, frames, 32, &count); /* drain */

    env.mock.battery.capacity_pct = 20;
    osd_tick(&env.daemon, 30);
    env_recv_frames(&env, frames, 32, &count);

    env.mock.battery.capacity_pct = 10;
    osd_tick(&env.daemon, 30);
    env_recv_frames(&env, frames, 32, &count);

    env.mock.battery.capacity_pct = 3;
    osd_tick(&env.daemon, 30);
    env_recv_frames(&env, frames, 32, &count);

    ASSERT(env.daemon.sm.mode == OSP_MODE_EMRG, "reached EMRG");
    ASSERT(env.daemon.samples_taken == 4, "4 samples");
    ASSERT(env.daemon.c0_signals_sent == 4, "4 C0 signals");
    ASSERT(env.daemon.records_emitted == 8, "8 records (4 MODE_CHANGE + 4 GATE)");
    ASSERT(env.mock.cpu_calls == 4, "4 CPU applications");
    ASSERT(env.mock.gate_calls == 4, "4 gate applications");

    env_teardown(&env);
}

static void test_status_command(void)
{
    test_env_t env;
    zbu_frame_t frames[8];
    size_t count;

    printf("test_status_command:\n");

    if (env_setup(&env) != 0) { ASSERT(0, "setup"); return; }

    /* Set some state */
    env.mock.battery.capacity_pct = 75;
    env.mock.thermal.temp_mc = 35000;
    osd_tick(&env.daemon, 30); /* update readings, no transition */

    /* Send STATUS command */
    uint8_t cmd[] = { OSD_CMD_STATUS };
    env_send_cmd(&env, cmd, 1);
    usleep(10000);
    osd_poll(&env.daemon);

    /* Receive status response */
    env_recv_frames(&env, frames, 8, &count);
    ASSERT(count == 1, "got status response");
    ASSERT(frames[0].data[0] == OSD_FRAME_STATUS, "is STATUS frame");
    ASSERT(frames[0].data[1] == OSP_MODE_FULL, "mode=FULL");
    ASSERT(frames[0].data[2] == OSP_GATE_FULL, "gate=none");
    ASSERT(frames[0].data[3] == 75, "batt=75%");

    env_teardown(&env);
}

static void test_force_mode_command(void)
{
    test_env_t env;
    zbu_frame_t frames[8];
    size_t count;

    printf("test_force_mode_command:\n");

    if (env_setup(&env) != 0) { ASSERT(0, "setup"); return; }

    /* Force to CONSERVATION */
    uint8_t cmd[] = { OSD_CMD_FORCE_MODE, OSP_MODE_CONS };
    env_send_cmd(&env, cmd, 2);
    usleep(10000);
    osd_poll(&env.daemon);

    ASSERT(env.daemon.sm.mode == OSP_MODE_CONS, "forced to CONS");
    ASSERT(env.daemon.sm.gate_mask == OSP_GATE_CONS, "gate mask updated");
    ASSERT(env.mock.last_cpu_mode == OSP_MODE_CONS, "CPU applied for CONS");
    ASSERT(env.mock.last_gate_mask == OSP_GATE_CONS, "gate applied for CONS");

    /* Should have emitted C0 + MODE_CHANGE */
    env_recv_frames(&env, frames, 8, &count);
    ASSERT(count == 2, "C0 + MODE_CHANGE emitted on force");

    env_teardown(&env);
}

static void test_thermal_via_daemon(void)
{
    test_env_t env;
    zbu_frame_t frames[16];
    size_t count;

    printf("test_thermal_via_daemon:\n");

    if (env_setup(&env) != 0) { ASSERT(0, "setup"); return; }

    env.mock.battery.capacity_pct = 80;

    /* Temperature above critical — first tick (30s sustained) */
    env.mock.thermal.temp_mc = 46000;
    osd_tick(&env.daemon, 30);
    ASSERT(env.daemon.sm.mode == OSP_MODE_FULL, "not yet triggered (30s)");

    /* Second tick — 60s sustained → thermal override */
    osd_tick(&env.daemon, 30);
    ASSERT(env.daemon.sm.mode == OSP_MODE_EMRG, "thermal override → EMRG");

    env_recv_frames(&env, frames, 16, &count);
    ASSERT(count == 3, "C0 + MODE_CHANGE + GATE");

    /* Verify reason in MODE_CHANGE */
    size_t i;
    for (i = 0; i < count; i++) {
        if (frames[i].data[0] == OSD_FRAME_MODE_CHANGE) {
            ASSERT(frames[i].data[3] == OSP_REASON_THERMAL, "reason=THERMAL");
            break;
        }
    }

    env_teardown(&env);
}

static void test_null_hal(void)
{
    osd_daemon_t d;
    osd_hal_t hal;

    printf("test_null_hal:\n");

    memset(&hal, 0, sizeof(hal));
    hal.ctx = NULL;

    /* Init with no read functions */
    osd_init(&d, TEST_SOCKET_PATH, &hal);
    d.running = 1;

    int rc = osd_tick(&d, 30);
    ASSERT(rc == OSD_ERR_HAL, "tick fails with NULL read_battery");
}

static void test_stop_poll(void)
{
    osd_daemon_t d;
    osd_hal_t hal;
    mock_hal_ctx_t m;

    printf("test_stop_poll:\n");

    mock_init(&m);
    hal.read_battery = mock_read_battery;
    hal.read_thermal = mock_read_thermal;
    hal.apply_cpu = NULL;
    hal.apply_gate = NULL;
    hal.ctx = &m;

    osd_init(&d, TEST_SOCKET_PATH, &hal);
    d.running = 1;
    osd_stop(&d);
    ASSERT(d.running == 0, "stop sets running=0");
    ASSERT(osd_tick(&d, 30) == OSD_ERR_SHUTDOWN, "tick after stop");
    ASSERT(osd_poll(&d) == OSD_ERR_SHUTDOWN, "poll after stop");
}

/* ======================================================================== */

int main(void)
{
    printf("=== outstack-powerd daemon shell tests ===\n\n");

    test_init_start_shutdown();
    test_tick_no_transition();
    test_tick_with_transition();
    test_c0_signal_priority();
    test_multiple_transitions();
    test_status_command();
    test_force_mode_command();
    test_thermal_via_daemon();
    test_null_hal();
    test_stop_poll();

    printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? EXIT_SUCCESS : EXIT_FAILURE;
}
