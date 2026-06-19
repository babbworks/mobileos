/*
 * test_daemon.c — Unit tests for telux-ledgerd daemon shell
 *
 * Tests the daemon event loop, frame dispatch, validation,
 * ACK/NAK responses, batch lifecycle, and conservation enforcement.
 *
 * Uses a local bus server + daemon client in the same process
 * to simulate real IPC without needing a separate bus process.
 */

#include "ledgerd_daemon.h"
#include "../libzako-bus/zako_bus.h"
#include "../libzako-bitledger/zako_bitledger.h"
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

#define TEST_SOCKET_PATH "/tmp/zako_ledgerd_test.sock"
#define TEST_DB_PATH     ":memory:"

/* ========================================================================
 * TEST HARNESS — local bus server + sender client
 * ======================================================================== */

typedef struct {
    zbu_server_t server;
    zbu_conn_t   sender;     /* simulates another daemon sending to ledgerd */
    ldd_daemon_t daemon;
} test_env_t;

static int env_setup(test_env_t *env)
{
    int rc;

    /* Start bus server */
    zbu_server_init(&env->server);
    rc = zbu_server_start(&env->server, TEST_SOCKET_PATH);
    if (rc != ZBU_OK) { return -1; }

    /* Init and start daemon (connects to bus) */
    ldd_init(&env->daemon, TEST_SOCKET_PATH, TEST_DB_PATH);
    rc = ldd_start(&env->daemon);
    if (rc != LDD_OK) {
        zbu_server_stop(&env->server);
        return -1;
    }

    /* Accept daemon's connection on server side */
    usleep(10000);
    zbu_server_poll(&env->server, 100);

    /* Drain subscription frames from daemon */
    zbu_frame_t sub_frame;
    usleep(10000);
    zbu_server_poll(&env->server, 100);
    while (zbu_server_recv(&env->server, &sub_frame) == ZBU_OK) {
        /* discard subscription messages */
    }

    /* Connect sender client */
    zbu_client_init(&env->sender);
    rc = zbu_client_connect(&env->sender, TEST_SOCKET_PATH);
    if (rc != ZBU_OK) {
        ldd_shutdown(&env->daemon);
        zbu_server_stop(&env->server);
        return -1;
    }

    usleep(10000);
    zbu_server_poll(&env->server, 100);

    return 0;
}

static void env_teardown(test_env_t *env)
{
    ldd_shutdown(&env->daemon);
    zbu_client_disconnect(&env->sender);
    zbu_server_stop(&env->server);
    usleep(10000); /* let sockets close */
}

/*
 * Send a frame from the "sender" client through the server to the daemon.
 * The server relays it to the daemon's bus connection.
 */
static int env_send_to_daemon(test_env_t *env, const uint8_t *data, size_t len)
{
    size_t daemon_idx = (size_t)-1;
    size_t i;

    /* Find daemon's client slot in server (first connected) */
    for (i = 0; i < ZBU_MAX_CLIENTS; i++) {
        if (env->server.clients[i].state == ZBU_STATE_ACTIVE) {
            daemon_idx = i;
            break;
        }
    }
    if (daemon_idx == (size_t)-1) { return -1; }

    return zbu_server_send(&env->server, daemon_idx, data, len);
}

/*
 * After sending a frame, let daemon poll and process it.
 * Then retrieve the daemon's ACK/NAK response from the server.
 */
static int env_poll_daemon(test_env_t *env)
{
    usleep(10000);
    return ldd_poll(&env->daemon, 0);
}

/*
 * Read response frame that daemon sent back to bus.
 */
static int env_recv_response(test_env_t *env, zbu_frame_t *frame)
{
    usleep(10000);
    zbu_server_poll(&env->server, 100);
    return zbu_server_recv(&env->server, frame);
}

/* ======================================================================== */

static void test_init_start_shutdown(void)
{
    ldd_daemon_t daemon;
    zbu_server_t server;
    int rc;

    printf("test_init_start_shutdown:\n");

    /* NULL checks */
    ASSERT(ldd_init(NULL, "/tmp/x", "/tmp/y") == LDD_ERR_NULL, "init NULL daemon");
    ASSERT(ldd_init(&daemon, NULL, "/tmp/y") == LDD_ERR_NULL, "init NULL socket");
    ASSERT(ldd_init(&daemon, "/tmp/x", NULL) == LDD_ERR_NULL, "init NULL db");

    /* Init succeeds */
    rc = ldd_init(&daemon, TEST_SOCKET_PATH, TEST_DB_PATH);
    ASSERT(rc == LDD_OK, "init OK");
    ASSERT(daemon.running == 0, "not running after init");
    ASSERT(daemon.header_valid == 0, "no header yet");
    ASSERT(daemon.batch_open == 0, "no batch yet");

    /* Start without server → connection failure */
    rc = ldd_start(&daemon);
    ASSERT(rc == LDD_ERR_BUS, "start fails without server");

    /* Start bus server, then start daemon */
    zbu_server_init(&server);
    zbu_server_start(&server, TEST_SOCKET_PATH);

    ldd_init(&daemon, TEST_SOCKET_PATH, TEST_DB_PATH);
    rc = ldd_start(&daemon);
    ASSERT(rc == LDD_OK, "start OK with server");
    ASSERT(daemon.running == 1, "running after start");

    /* Shutdown */
    ldd_shutdown(&daemon);
    ASSERT(daemon.running == 0, "not running after shutdown");

    zbu_server_stop(&server);
    usleep(10000);
}

static void test_control_session_open_close(void)
{
    test_env_t env;
    zbu_frame_t resp;
    zbl_control_t ctrl;

    printf("test_control_session_open_close:\n");

    if (env_setup(&env) != 0) {
        ASSERT(0, "env_setup failed");
        return;
    }

    /* Send SESSION_OPEN control */
    uint8_t session_open = zbl_control_encode(ZBL_CTRL_SESSION_OPEN, 0);
    env_send_to_daemon(&env, &session_open, 1);
    env_poll_daemon(&env);

    /* Should get ACK back */
    int rc = env_recv_response(&env, &resp);
    ASSERT(rc == ZBU_OK, "got response to session_open");
    ASSERT(resp.len == 1, "response is 1 byte");
    zbl_control_decode(resp.data[0], &ctrl);
    ASSERT(ctrl.type == ZBL_CTRL_ACK, "response is ACK");

    /* Send SESSION_CLOSE */
    uint8_t session_close = zbl_control_encode(ZBL_CTRL_SESSION_CLOSE, 0);
    env_send_to_daemon(&env, &session_close, 1);
    env_poll_daemon(&env);

    rc = env_recv_response(&env, &resp);
    ASSERT(rc == ZBU_OK, "got response to session_close");
    zbl_control_decode(resp.data[0], &ctrl);
    ASSERT(ctrl.type == ZBL_CTRL_ACK, "session_close ACKed");

    env_teardown(&env);
}

static void test_control_batch_lifecycle(void)
{
    test_env_t env;
    zbu_frame_t resp;
    zbl_control_t ctrl;

    printf("test_control_batch_lifecycle:\n");

    if (env_setup(&env) != 0) {
        ASSERT(0, "env_setup failed");
        return;
    }

    /* Open batch */
    uint8_t batch_open = zbl_control_encode(ZBL_CTRL_BATCH_OPEN, 0);
    env_send_to_daemon(&env, &batch_open, 1);
    env_poll_daemon(&env);

    int rc = env_recv_response(&env, &resp);
    ASSERT(rc == ZBU_OK, "batch_open response");
    zbl_control_decode(resp.data[0], &ctrl);
    ASSERT(ctrl.type == ZBL_CTRL_ACK, "batch_open ACKed");
    ASSERT(env.daemon.batch_open == 1, "batch is open");

    /* Close batch (empty batch — balance=0, conserved) */
    uint8_t batch_close = zbl_control_encode(ZBL_CTRL_BATCH_CLOSE, 0);
    env_send_to_daemon(&env, &batch_close, 1);
    env_poll_daemon(&env);

    rc = env_recv_response(&env, &resp);
    ASSERT(rc == ZBU_OK, "batch_close response");
    zbl_control_decode(resp.data[0], &ctrl);
    ASSERT(ctrl.type == ZBL_CTRL_ACK, "empty batch conserved → ACK");
    ASSERT(env.daemon.batch_open == 0, "batch closed");
    ASSERT(env.daemon.batches_closed == 1, "batch count incremented");
    ASSERT(env.daemon.batches_conserved == 1, "conserved count incremented");

    env_teardown(&env);
}

static void test_batch_close_no_batch(void)
{
    test_env_t env;
    zbu_frame_t resp;
    zbl_control_t ctrl;

    printf("test_batch_close_no_batch:\n");

    if (env_setup(&env) != 0) {
        ASSERT(0, "env_setup failed");
        return;
    }

    /* Close batch without opening → NAK */
    uint8_t batch_close = zbl_control_encode(ZBL_CTRL_BATCH_CLOSE, 0);
    env_send_to_daemon(&env, &batch_close, 1);
    env_poll_daemon(&env);

    int rc = env_recv_response(&env, &resp);
    ASSERT(rc == ZBU_OK, "got NAK response");
    zbl_control_decode(resp.data[0], &ctrl);
    ASSERT(ctrl.type == ZBL_CTRL_NAK, "batch_close without open → NAK");
    ASSERT(ctrl.payload == 0x06, "reason = no batch open");

    env_teardown(&env);
}

static void test_layer2_header(void)
{
    test_env_t env;
    zbu_frame_t resp;
    zbl_control_t ctrl;
    zbl_layer2_t header;
    uint8_t header_buf[ZBL_LAYER2_SIZE];

    printf("test_layer2_header:\n");

    if (env_setup(&env) != 0) {
        ASSERT(0, "env_setup failed");
        return;
    }

    /* Encode a valid Layer 2 header */
    memset(&header, 0, sizeof(header));
    header.transmission_type = 2;
    header.scaling_factor = 2;
    header.optimal_split = 8;
    header.decimal_pos = 2;
    header.currency_code = 5;
    zbl_layer2_encode(&header, header_buf);

    /* Send it */
    env_send_to_daemon(&env, header_buf, ZBL_LAYER2_SIZE);
    env_poll_daemon(&env);

    int rc = env_recv_response(&env, &resp);
    ASSERT(rc == ZBU_OK, "layer2 response");
    zbl_control_decode(resp.data[0], &ctrl);
    ASSERT(ctrl.type == ZBL_CTRL_ACK, "layer2 header ACKed");
    ASSERT(env.daemon.header_valid == 1, "header now valid");
    ASSERT(env.daemon.current_header.optimal_split == 8, "split=8 stored");

    env_teardown(&env);
}

static void test_layer3_record_accepted(void)
{
    test_env_t env;
    zbu_frame_t resp;
    zbl_control_t ctrl;
    uint8_t record_buf[ZBL_LAYER3_SIZE];

    printf("test_layer3_record_accepted:\n");

    if (env_setup(&env) != 0) {
        ASSERT(0, "env_setup failed");
        return;
    }

    /* Open batch first */
    uint8_t batch_open = zbl_control_encode(ZBL_CTRL_BATCH_OPEN, 0);
    env_send_to_daemon(&env, &batch_open, 1);
    env_poll_daemon(&env);
    env_recv_response(&env, &resp); /* drain ACK */

    /* Encode a valid Layer 3 record: $4.53, Plus/In, Paid, Credit */
    zbl_record_encode(453, 0, 0, 0, 0, 0, 0, 0,
                      ZBL_AP_OP_INCOME_ASSET, 0, 0, record_buf);

    /* Send it */
    env_send_to_daemon(&env, record_buf, ZBL_LAYER3_SIZE);
    env_poll_daemon(&env);

    int rc = env_recv_response(&env, &resp);
    ASSERT(rc == ZBU_OK, "layer3 response");
    zbl_control_decode(resp.data[0], &ctrl);
    ASSERT(ctrl.type == ZBL_CTRL_ACK, "valid record ACKed");
    ASSERT(env.daemon.records_accepted == 1, "accepted count = 1");
    ASSERT(env.daemon.batch_record_count == 1, "batch tracks 1 record");

    env_teardown(&env);
}

static void test_layer3_crosslayer_reject(void)
{
    test_env_t env;
    zbu_frame_t resp;
    zbl_control_t ctrl;
    uint8_t record_buf[ZBL_LAYER3_SIZE];

    printf("test_layer3_crosslayer_reject:\n");

    if (env_setup(&env) != 0) {
        ASSERT(0, "env_setup failed");
        return;
    }

    /* Open batch */
    uint8_t batch_open = zbl_control_encode(ZBL_CTRL_BATCH_OPEN, 0);
    env_send_to_daemon(&env, &batch_open, 1);
    env_poll_daemon(&env);
    env_recv_response(&env, &resp);

    /* Encode valid record then corrupt cross-layer echo */
    zbl_record_encode(1000, 0, 0, 0, 1, 0, 1, 0,
                      ZBL_AP_ASSET_ASSET, 0, 0, record_buf);
    /* Flip bit 37 (dir_echo) — byte4, bit3 */
    record_buf[4] ^= 0x08u;

    env_send_to_daemon(&env, record_buf, ZBL_LAYER3_SIZE);
    env_poll_daemon(&env);

    int rc = env_recv_response(&env, &resp);
    ASSERT(rc == ZBU_OK, "got crosslayer NAK");
    zbl_control_decode(resp.data[0], &ctrl);
    ASSERT(ctrl.type == ZBL_CTRL_NAK, "crosslayer failure → NAK");
    ASSERT(ctrl.payload == 0x01, "reason = crosslayer");
    ASSERT(env.daemon.records_rejected == 1, "rejected count = 1");

    env_teardown(&env);
}

static void test_layer3_rounding_reject(void)
{
    test_env_t env;
    zbu_frame_t resp;
    zbl_control_t ctrl;
    uint8_t record_buf[ZBL_LAYER3_SIZE];

    printf("test_layer3_rounding_reject:\n");

    if (env_setup(&env) != 0) {
        ASSERT(0, "env_setup failed");
        return;
    }

    /* Open batch */
    uint8_t batch_open = zbl_control_encode(ZBL_CTRL_BATCH_OPEN, 0);
    env_send_to_daemon(&env, &batch_open, 1);
    env_poll_daemon(&env);
    env_recv_response(&env, &resp);

    /* Encode record with rounding=0 then set round_dir=1 (protocol error) */
    zbl_record_encode(100, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, record_buf);
    /* bit 27 (round_dir): byte 3, bit pos = 7-(26%8) = 7-2 = 5 → 0x20 */
    record_buf[3] |= 0x20u;

    env_send_to_daemon(&env, record_buf, ZBL_LAYER3_SIZE);
    env_poll_daemon(&env);

    int rc = env_recv_response(&env, &resp);
    ASSERT(rc == ZBU_OK, "got rounding NAK");
    zbl_control_decode(resp.data[0], &ctrl);
    ASSERT(ctrl.type == ZBL_CTRL_NAK, "rounding failure → NAK");
    ASSERT(ctrl.payload == 0x02, "reason = rounding");

    env_teardown(&env);
}

static void test_layer3_no_batch_reject(void)
{
    test_env_t env;
    zbu_frame_t resp;
    zbl_control_t ctrl;
    uint8_t record_buf[ZBL_LAYER3_SIZE];

    printf("test_layer3_no_batch_reject:\n");

    if (env_setup(&env) != 0) {
        ASSERT(0, "env_setup failed");
        return;
    }

    /* Send record without opening batch */
    zbl_record_encode(500, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, record_buf);
    env_send_to_daemon(&env, record_buf, ZBL_LAYER3_SIZE);
    env_poll_daemon(&env);

    int rc = env_recv_response(&env, &resp);
    ASSERT(rc == ZBU_OK, "got no-batch NAK");
    zbl_control_decode(resp.data[0], &ctrl);
    ASSERT(ctrl.type == ZBL_CTRL_NAK, "no batch → NAK");
    ASSERT(ctrl.payload == 0x06, "reason = no batch open");

    env_teardown(&env);
}

static void test_conservation_pass(void)
{
    test_env_t env;
    zbu_frame_t resp;
    zbl_control_t ctrl;
    uint8_t record_buf[ZBL_LAYER3_SIZE];

    printf("test_conservation_pass:\n");

    if (env_setup(&env) != 0) {
        ASSERT(0, "env_setup failed");
        return;
    }

    /* Open batch */
    uint8_t batch_open = zbl_control_encode(ZBL_CTRL_BATCH_OPEN, 0);
    env_send_to_daemon(&env, &batch_open, 1);
    env_poll_daemon(&env);
    env_recv_response(&env, &resp);

    /* +500 (Plus/In) */
    zbl_record_encode(500, 0, 0, 0, 0, 0, 0, 0,
                      ZBL_AP_OP_INCOME_ASSET, 0, 0, record_buf);
    env_send_to_daemon(&env, record_buf, ZBL_LAYER3_SIZE);
    env_poll_daemon(&env);
    env_recv_response(&env, &resp);

    /* +300 (Plus/In) */
    zbl_record_encode(300, 0, 0, 0, 0, 0, 0, 0,
                      ZBL_AP_OP_INCOME_ASSET, 0, 0, record_buf);
    env_send_to_daemon(&env, record_buf, ZBL_LAYER3_SIZE);
    env_poll_daemon(&env);
    env_recv_response(&env, &resp);

    /* -600 (Minus/Out) */
    zbl_record_encode(600, 0, 0, 0, 1, 0, 1, 0,
                      ZBL_AP_OP_EXPENSE_ASSET, 0, 0, record_buf);
    env_send_to_daemon(&env, record_buf, ZBL_LAYER3_SIZE);
    env_poll_daemon(&env);
    env_recv_response(&env, &resp);

    /* -200 (Minus/Out) */
    zbl_record_encode(200, 0, 0, 0, 1, 0, 1, 0,
                      ZBL_AP_OP_EXPENSE_ASSET, 0, 0, record_buf);
    env_send_to_daemon(&env, record_buf, ZBL_LAYER3_SIZE);
    env_poll_daemon(&env);
    env_recv_response(&env, &resp);

    /* Close batch — should be conserved (500+300-600-200=0) */
    uint8_t batch_close = zbl_control_encode(ZBL_CTRL_BATCH_CLOSE, 0);
    env_send_to_daemon(&env, &batch_close, 1);
    env_poll_daemon(&env);

    int rc = env_recv_response(&env, &resp);
    ASSERT(rc == ZBU_OK, "conservation response");
    zbl_control_decode(resp.data[0], &ctrl);
    ASSERT(ctrl.type == ZBL_CTRL_ACK, "balanced batch → ACK");
    ASSERT(env.daemon.batches_conserved == 1, "conserved count = 1");
    ASSERT(env.daemon.records_accepted == 4, "4 records accepted");

    env_teardown(&env);
}

static void test_conservation_fail(void)
{
    test_env_t env;
    zbu_frame_t resp;
    zbl_control_t ctrl;
    uint8_t record_buf[ZBL_LAYER3_SIZE];

    printf("test_conservation_fail:\n");

    if (env_setup(&env) != 0) {
        ASSERT(0, "env_setup failed");
        return;
    }

    /* Open batch */
    uint8_t batch_open = zbl_control_encode(ZBL_CTRL_BATCH_OPEN, 0);
    env_send_to_daemon(&env, &batch_open, 1);
    env_poll_daemon(&env);
    env_recv_response(&env, &resp);

    /* +1000 (Plus/In) */
    zbl_record_encode(1000, 0, 0, 0, 0, 0, 0, 0,
                      ZBL_AP_OP_INCOME_ASSET, 0, 0, record_buf);
    env_send_to_daemon(&env, record_buf, ZBL_LAYER3_SIZE);
    env_poll_daemon(&env);
    env_recv_response(&env, &resp);

    /* -999 (Minus/Out) — imbalance of +1 */
    zbl_record_encode(999, 0, 0, 0, 1, 0, 1, 0,
                      ZBL_AP_OP_EXPENSE_ASSET, 0, 0, record_buf);
    env_send_to_daemon(&env, record_buf, ZBL_LAYER3_SIZE);
    env_poll_daemon(&env);
    env_recv_response(&env, &resp);

    /* Close batch — should be REJECTED (1000-999=1 ≠ 0) */
    uint8_t batch_close = zbl_control_encode(ZBL_CTRL_BATCH_CLOSE, 0);
    env_send_to_daemon(&env, &batch_close, 1);
    env_poll_daemon(&env);

    int rc = env_recv_response(&env, &resp);
    ASSERT(rc == ZBU_OK, "conservation fail response");
    zbl_control_decode(resp.data[0], &ctrl);
    ASSERT(ctrl.type == ZBL_CTRL_NAK, "imbalanced batch → NAK");
    ASSERT(ctrl.payload == 0x04, "reason = conservation failure");
    ASSERT(env.daemon.batches_closed == 1, "batch counted as closed attempt");
    ASSERT(env.daemon.batches_conserved == 0, "not conserved");

    env_teardown(&env);
}

static void test_unknown_frame_size(void)
{
    test_env_t env;
    zbu_frame_t resp;
    zbl_control_t ctrl;

    printf("test_unknown_frame_size:\n");

    if (env_setup(&env) != 0) {
        ASSERT(0, "env_setup failed");
        return;
    }

    /* Send 3 bytes (not a valid frame size) */
    uint8_t garbage[] = { 0xAA, 0xBB, 0xCC };
    env_send_to_daemon(&env, garbage, 3);
    env_poll_daemon(&env);

    int rc = env_recv_response(&env, &resp);
    ASSERT(rc == ZBU_OK, "got response to garbage");
    zbl_control_decode(resp.data[0], &ctrl);
    ASSERT(ctrl.type == ZBL_CTRL_NAK, "unknown size → NAK");
    ASSERT(ctrl.payload == 0x05, "reason = parse error");

    env_teardown(&env);
}

static void test_combined_header_record(void)
{
    test_env_t env;
    zbu_frame_t resp;
    zbl_control_t ctrl;
    zbl_layer2_t header;
    uint8_t combined[ZBL_LAYER2_SIZE + ZBL_LAYER3_SIZE];

    printf("test_combined_header_record:\n");

    if (env_setup(&env) != 0) {
        ASSERT(0, "env_setup failed");
        return;
    }

    /* Open batch first */
    uint8_t batch_open = zbl_control_encode(ZBL_CTRL_BATCH_OPEN, 0);
    env_send_to_daemon(&env, &batch_open, 1);
    env_poll_daemon(&env);
    env_recv_response(&env, &resp);

    /* Build combined 11-byte frame: Layer 2 + Layer 3 */
    memset(&header, 0, sizeof(header));
    header.transmission_type = 1;
    header.scaling_factor = 2;
    header.optimal_split = 8;
    header.decimal_pos = 2;
    header.currency_code = 5;
    zbl_layer2_encode(&header, combined);
    zbl_record_encode(750, 0, 0, 0, 0, 0, 0, 0,
                      ZBL_AP_OP_INCOME_ASSET, 0, 0,
                      combined + ZBL_LAYER2_SIZE);

    /* Send combined */
    env_send_to_daemon(&env, combined, sizeof(combined));
    env_poll_daemon(&env);

    /* Should get ACK for header */
    int rc = env_recv_response(&env, &resp);
    ASSERT(rc == ZBU_OK, "combined: got first response");
    zbl_control_decode(resp.data[0], &ctrl);
    ASSERT(ctrl.type == ZBL_CTRL_ACK, "combined: header ACKed");

    /* Should get ACK for record */
    rc = env_recv_response(&env, &resp);
    ASSERT(rc == ZBU_OK, "combined: got second response");
    zbl_control_decode(resp.data[0], &ctrl);
    ASSERT(ctrl.type == ZBL_CTRL_ACK, "combined: record ACKed");

    ASSERT(env.daemon.header_valid == 1, "header set from combined");
    ASSERT(env.daemon.records_accepted == 1, "record accepted from combined");

    env_teardown(&env);
}

static void test_stats_tracking(void)
{
    test_env_t env;
    zbu_frame_t resp;
    uint8_t record_buf[ZBL_LAYER3_SIZE];

    printf("test_stats_tracking:\n");

    if (env_setup(&env) != 0) {
        ASSERT(0, "env_setup failed");
        return;
    }

    ASSERT(env.daemon.frames_received == 0, "initial frames = 0");
    ASSERT(env.daemon.records_accepted == 0, "initial accepted = 0");
    ASSERT(env.daemon.records_rejected == 0, "initial rejected = 0");

    /* Open batch */
    uint8_t batch_open = zbl_control_encode(ZBL_CTRL_BATCH_OPEN, 0);
    env_send_to_daemon(&env, &batch_open, 1);
    env_poll_daemon(&env);
    env_recv_response(&env, &resp);

    /* Send valid record */
    zbl_record_encode(100, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, record_buf);
    env_send_to_daemon(&env, record_buf, ZBL_LAYER3_SIZE);
    env_poll_daemon(&env);
    env_recv_response(&env, &resp);

    /* Send another valid record */
    zbl_record_encode(200, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, record_buf);
    env_send_to_daemon(&env, record_buf, ZBL_LAYER3_SIZE);
    env_poll_daemon(&env);
    env_recv_response(&env, &resp);

    /* Send invalid (garbage size) */
    uint8_t garbage[] = { 0x01, 0x02 };
    env_send_to_daemon(&env, garbage, 2);
    env_poll_daemon(&env);
    env_recv_response(&env, &resp);

    ASSERT(env.daemon.frames_received == 4, "4 frames total (batch_open + 2 records + garbage)");
    ASSERT(env.daemon.records_accepted == 2, "2 accepted");
    /* garbage frame triggers NAK but records_rejected only counts Layer 3 rejects */

    env_teardown(&env);
}

static void test_stop_and_poll(void)
{
    ldd_daemon_t daemon;
    int rc;

    printf("test_stop_and_poll:\n");

    ldd_init(&daemon, TEST_SOCKET_PATH, TEST_DB_PATH);
    daemon.running = 1;

    ldd_stop(&daemon);
    ASSERT(daemon.running == 0, "stop sets running=0");

    rc = ldd_poll(&daemon, 0);
    ASSERT(rc == LDD_ERR_SHUTDOWN, "poll after stop returns SHUTDOWN");

    /* NULL poll */
    ASSERT(ldd_poll(NULL, 0) == LDD_ERR_NULL, "poll NULL");
}

/* ======================================================================== */

int main(void)
{
    printf("=== telux-ledgerd daemon shell tests ===\n\n");

    test_init_start_shutdown();
    test_control_session_open_close();
    test_control_batch_lifecycle();
    test_batch_close_no_batch();
    test_layer2_header();
    test_layer3_record_accepted();
    test_layer3_crosslayer_reject();
    test_layer3_rounding_reject();
    test_layer3_no_batch_reject();
    test_conservation_pass();
    test_conservation_fail();
    test_unknown_frame_size();
    test_combined_header_record();
    test_stats_tracking();
    test_stop_and_poll();

    printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? EXIT_SUCCESS : EXIT_FAILURE;
}
