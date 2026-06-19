/*
 * test_exchange.c — Unit tests for Exchange Engine
 *
 * Tests bilateral exchange: SEND/RECEIVE, conservation enforcement,
 * signature verification, atomic posting, cancellation, state machine.
 */

#include "exchange_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* ========================================================================
 * MOCK CALLBACKS
 * ======================================================================== */

static int mock_post_count = 0;
static int mock_post_fail = 0;

static int mock_post_atomic(const uint8_t *frame_a, size_t len_a,
                            const uint8_t *frame_b, size_t len_b,
                            uint32_t sender_id, void *ctx)
{
    (void)frame_a; (void)len_a; (void)frame_b; (void)len_b;
    (void)sender_id; (void)ctx;
    mock_post_count++;
    return mock_post_fail ? -1 : 0;
}

static int mock_verify_fail = 0;

static int mock_verify_sig(const char *did,
                           const uint8_t *message, size_t msg_len,
                           const uint8_t *sig, void *ctx)
{
    (void)did; (void)message; (void)msg_len; (void)sig; (void)ctx;
    return mock_verify_fail ? -1 : 0;
}

static void reset_mocks(void)
{
    mock_post_count = 0;
    mock_post_fail = 0;
    mock_verify_fail = 0;
}

static exe_callbacks_t make_callbacks(void)
{
    exe_callbacks_t cb;
    cb.post_atomic = mock_post_atomic;
    cb.verify_sig = mock_verify_sig;
    cb.ctx = NULL;
    return cb;
}

/* Fake frame data */
static uint8_t FRAME_SEND[] = { 0x80, 0x01, 0xF4, 0x00, 0x04 }; /* mock 5-byte */
static uint8_t FRAME_ACK[]  = { 0x80, 0x01, 0xF4, 0x01, 0x04 }; /* mock ACK */
static uint8_t FAKE_SIG[64];

/* ======================================================================== */

static void test_init(void)
{
    exe_engine_t engine;
    exe_callbacks_t cb = make_callbacks();

    printf("test_init:\n");

    int rc = exe_init(&engine, &cb);
    ASSERT(rc == EXE_OK, "init OK");
    ASSERT(engine.next_id == 1, "next_id starts at 1");
    ASSERT(engine.completed_count == 0, "no completions");
    ASSERT(exe_get_pending_count(&engine) == 0, "no pending");

    ASSERT(exe_init(NULL, &cb) == EXE_ERR_NULL, "NULL engine");
    ASSERT(exe_init(&engine, NULL) == EXE_ERR_NULL, "NULL callbacks");
}

static void test_send_creates_outbound(void)
{
    exe_engine_t engine;
    exe_callbacks_t cb = make_callbacks();
    uint32_t id;
    uint8_t state;

    printf("test_send_creates_outbound:\n");

    reset_mocks();
    exe_init(&engine, &cb);
    memset(FAKE_SIG, 0xAA, 64);

    int rc = exe_send(&engine, FRAME_SEND, 5, "did:key:zAlice",
                      FAKE_SIG, "did:key:zBob", 500, &id);
    ASSERT(rc == EXE_OK, "send OK");
    ASSERT(id == 1, "first exchange gets id=1");

    rc = exe_get_state(&engine, id, &state);
    ASSERT(rc == EXE_OK, "get_state OK");
    ASSERT(state == EXE_STATE_PENDING_OUTBOUND, "state = PENDING_OUTBOUND");
    ASSERT(exe_get_pending_count(&engine) == 1, "1 pending");
}

static void test_full_outbound_exchange(void)
{
    exe_engine_t engine;
    exe_callbacks_t cb = make_callbacks();
    uint32_t id;
    uint8_t state;

    printf("test_full_outbound_exchange:\n");

    reset_mocks();
    exe_init(&engine, &cb);
    memset(FAKE_SIG, 0xBB, 64);

    /* Alice sends 500 to Bob (outflow from Alice) */
    exe_send(&engine, FRAME_SEND, 5, "did:key:zAlice",
             FAKE_SIG, "did:key:zBob", 500, &id);

    /* Bob ACKs with a matching inflow of 500 (from Alice's perspective: inflow) */
    int rc = exe_complete_outbound(&engine, id, FRAME_ACK, 5,
                                   "did:key:zBob", FAKE_SIG, 500, 0);
    ASSERT(rc == EXE_OK, "complete_outbound OK");

    exe_get_state(&engine, id, &state);
    ASSERT(state == EXE_STATE_COMPLETED, "state = COMPLETED");
    ASSERT(mock_post_count == 1, "atomic post called once");
    ASSERT(engine.completed_count == 1, "completed count = 1");
    ASSERT(exe_get_pending_count(&engine) == 0, "0 pending after completion");
}

static void test_full_inbound_exchange(void)
{
    exe_engine_t engine;
    exe_callbacks_t cb = make_callbacks();
    uint32_t id;
    uint8_t state;

    printf("test_full_inbound_exchange:\n");

    reset_mocks();
    exe_init(&engine, &cb);
    memset(FAKE_SIG, 0xCC, 64);

    /* Bob sends to us: we receive an inflow of 300 */
    int rc = exe_receive(&engine, FRAME_SEND, 5, "did:key:zBob",
                         FAKE_SIG, 300, 0, &id);
    ASSERT(rc == EXE_OK, "receive OK");

    exe_get_state(&engine, id, &state);
    ASSERT(state == EXE_STATE_PENDING_INBOUND, "PENDING_INBOUND");

    /* We acknowledge with a matching outflow of 300 */
    rc = exe_acknowledge(&engine, id, FRAME_ACK, 5,
                         "did:key:zAlice", FAKE_SIG, 300, 1);
    ASSERT(rc == EXE_OK, "acknowledge OK");

    exe_get_state(&engine, id, &state);
    ASSERT(state == EXE_STATE_COMPLETED, "COMPLETED");
    ASSERT(mock_post_count == 1, "posted atomically");
}

static void test_conservation_failure(void)
{
    exe_engine_t engine;
    exe_callbacks_t cb = make_callbacks();
    uint32_t id;
    uint8_t state;

    printf("test_conservation_failure:\n");

    reset_mocks();
    exe_init(&engine, &cb);
    memset(FAKE_SIG, 0xDD, 64);

    /* Receive inflow of 500 from Bob */
    exe_receive(&engine, FRAME_SEND, 5, "did:key:zBob",
                FAKE_SIG, 500, 0, &id);

    /* Try to ACK with only 499 outflow — conservation fails */
    int rc = exe_acknowledge(&engine, id, FRAME_ACK, 5,
                             "did:key:zAlice", FAKE_SIG, 499, 1);
    ASSERT(rc == EXE_ERR_CONSERVATION, "imbalanced → CONSERVATION error");

    exe_get_state(&engine, id, &state);
    ASSERT(state == EXE_STATE_FAILED, "state = FAILED");
    ASSERT(mock_post_count == 0, "no post on conservation failure");
    ASSERT(engine.failed_count == 1, "failed count = 1");
}

static void test_conservation_outbound_failure(void)
{
    exe_engine_t engine;
    exe_callbacks_t cb = make_callbacks();
    uint32_t id;
    uint8_t state;

    printf("test_conservation_outbound_failure:\n");

    reset_mocks();
    exe_init(&engine, &cb);
    memset(FAKE_SIG, 0xEE, 64);

    /* We send 1000 out */
    exe_send(&engine, FRAME_SEND, 5, "did:key:zAlice",
             FAKE_SIG, "did:key:zBob", 1000, &id);

    /* Bob responds with only 999 inflow — imbalanced */
    int rc = exe_complete_outbound(&engine, id, FRAME_ACK, 5,
                                   "did:key:zBob", FAKE_SIG, 999, 0);
    ASSERT(rc == EXE_ERR_CONSERVATION, "outbound conservation failure");

    exe_get_state(&engine, id, &state);
    ASSERT(state == EXE_STATE_FAILED, "FAILED");
}

static void test_signature_verification_failure(void)
{
    exe_engine_t engine;
    exe_callbacks_t cb = make_callbacks();
    uint32_t id;

    printf("test_signature_verification_failure:\n");

    reset_mocks();
    mock_verify_fail = 1;
    exe_init(&engine, &cb);
    memset(FAKE_SIG, 0xFF, 64);

    /* Receive with bad signature → rejected */
    int rc = exe_receive(&engine, FRAME_SEND, 5, "did:key:zEvil",
                         FAKE_SIG, 500, 0, &id);
    ASSERT(rc == EXE_ERR_VERIFY, "bad sig rejected");
    ASSERT(exe_get_pending_count(&engine) == 0, "nothing pending");
}

static void test_cancel(void)
{
    exe_engine_t engine;
    exe_callbacks_t cb = make_callbacks();
    uint32_t id;
    uint8_t state;

    printf("test_cancel:\n");

    reset_mocks();
    exe_init(&engine, &cb);
    memset(FAKE_SIG, 0x11, 64);

    exe_send(&engine, FRAME_SEND, 5, "did:key:zAlice",
             FAKE_SIG, "did:key:zBob", 100, &id);

    int rc = exe_cancel(&engine, id);
    ASSERT(rc == EXE_OK, "cancel OK");

    exe_get_state(&engine, id, &state);
    ASSERT(state == EXE_STATE_CANCELLED, "state = CANCELLED");
    ASSERT(engine.cancelled_count == 1, "cancelled count");

    /* Can't cancel a completed exchange */
    /* (first let's make a completed one) */
    uint32_t id2;
    exe_send(&engine, FRAME_SEND, 5, "did:key:zAlice",
             FAKE_SIG, "did:key:zBob", 200, &id2);
    exe_complete_outbound(&engine, id2, FRAME_ACK, 5,
                          "did:key:zBob", FAKE_SIG, 200, 0);
    rc = exe_cancel(&engine, id2);
    ASSERT(rc == EXE_ERR_STATE, "can't cancel completed");
}

static void test_post_failure(void)
{
    exe_engine_t engine;
    exe_callbacks_t cb = make_callbacks();
    uint32_t id;
    uint8_t state;

    printf("test_post_failure:\n");

    reset_mocks();
    mock_post_fail = 1;
    exe_init(&engine, &cb);
    memset(FAKE_SIG, 0x22, 64);

    /* Full exchange but post fails */
    exe_send(&engine, FRAME_SEND, 5, "did:key:zAlice",
             FAKE_SIG, "did:key:zBob", 750, &id);

    int rc = exe_complete_outbound(&engine, id, FRAME_ACK, 5,
                                   "did:key:zBob", FAKE_SIG, 750, 0);
    ASSERT(rc == EXE_ERR_POST, "post failure propagated");

    exe_get_state(&engine, id, &state);
    ASSERT(state == EXE_STATE_FAILED, "FAILED on post error");
}

static void test_receive_matches_outbound(void)
{
    exe_engine_t engine;
    exe_callbacks_t cb = make_callbacks();
    uint32_t send_id, recv_id;

    printf("test_receive_matches_outbound:\n");

    reset_mocks();
    exe_init(&engine, &cb);
    memset(FAKE_SIG, 0x33, 64);

    /* We send to Bob */
    exe_send(&engine, FRAME_SEND, 5, "did:key:zAlice",
             FAKE_SIG, "did:key:zBob", 400, &send_id);
    ASSERT(exe_get_pending_count(&engine) == 1, "1 pending outbound");

    /* Bob's response arrives via exe_receive — should match our outbound */
    int rc = exe_receive(&engine, FRAME_ACK, 5, "did:key:zBob",
                         FAKE_SIG, 400, 0, &recv_id);
    ASSERT(rc == EXE_OK, "receive matched to outbound");
    ASSERT(recv_id == send_id, "matched exchange has same ID");

    /* Now complete it */
    rc = exe_complete_outbound(&engine, send_id, FRAME_ACK, 5,
                               "did:key:zBob", FAKE_SIG, 400, 0);
    ASSERT(rc == EXE_OK, "complete OK");
    ASSERT(engine.completed_count == 1, "completed");
}

static void test_multiple_exchanges(void)
{
    exe_engine_t engine;
    exe_callbacks_t cb = make_callbacks();
    uint32_t id1, id2, id3;

    printf("test_multiple_exchanges:\n");

    reset_mocks();
    exe_init(&engine, &cb);
    memset(FAKE_SIG, 0x44, 64);

    exe_send(&engine, FRAME_SEND, 5, "did:key:zAlice",
             FAKE_SIG, "did:key:zBob", 100, &id1);
    exe_send(&engine, FRAME_SEND, 5, "did:key:zAlice",
             FAKE_SIG, "did:key:zCharlie", 200, &id2);
    exe_send(&engine, FRAME_SEND, 5, "did:key:zAlice",
             FAKE_SIG, "did:key:zDave", 300, &id3);

    ASSERT(exe_get_pending_count(&engine) == 3, "3 pending");
    ASSERT(id1 != id2 && id2 != id3, "unique IDs");

    /* Complete one */
    exe_complete_outbound(&engine, id2, FRAME_ACK, 5,
                          "did:key:zCharlie", FAKE_SIG, 200, 0);
    ASSERT(exe_get_pending_count(&engine) == 2, "2 pending after 1 complete");
}

static void test_exchange_table_full(void)
{
    exe_engine_t engine;
    exe_callbacks_t cb = make_callbacks();
    uint32_t id;
    int rc;
    size_t i;

    printf("test_exchange_table_full:\n");

    reset_mocks();
    exe_init(&engine, &cb);
    memset(FAKE_SIG, 0x55, 64);

    /* Fill all slots */
    for (i = 0; i < EXE_MAX_EXCHANGES; i++) {
        char did[32];
        snprintf(did, sizeof(did), "did:key:z%zu", i);
        rc = exe_send(&engine, FRAME_SEND, 5, "did:key:zAlice",
                      FAKE_SIG, did, 10, &id);
        ASSERT(rc == EXE_OK, "fill slot");
    }

    /* Next one should fail */
    rc = exe_send(&engine, FRAME_SEND, 5, "did:key:zAlice",
                  FAKE_SIG, "did:key:zOverflow", 10, &id);
    ASSERT(rc == EXE_ERR_FULL, "table full");
}

static void test_null_checks(void)
{
    exe_engine_t engine;
    exe_callbacks_t cb = make_callbacks();
    uint32_t id;
    uint8_t state;

    printf("test_null_checks:\n");

    exe_init(&engine, &cb);

    ASSERT(exe_send(NULL, FRAME_SEND, 5, "x", FAKE_SIG, "y", 1, &id) == EXE_ERR_NULL, "send NULL engine");
    ASSERT(exe_send(&engine, NULL, 5, "x", FAKE_SIG, "y", 1, &id) == EXE_ERR_NULL, "send NULL frame");
    ASSERT(exe_receive(NULL, FRAME_SEND, 5, "x", FAKE_SIG, 1, 0, &id) == EXE_ERR_NULL, "recv NULL");
    ASSERT(exe_acknowledge(NULL, 1, FRAME_ACK, 5, "x", FAKE_SIG, 1, 0) == EXE_ERR_NULL, "ack NULL");
    ASSERT(exe_cancel(NULL, 1) == EXE_ERR_NULL, "cancel NULL");
    ASSERT(exe_get_state(NULL, 1, &state) == EXE_ERR_NULL, "get_state NULL");
    ASSERT(exe_get_state(&engine, 999, &state) == EXE_ERR_NOT_FOUND, "not found");
}

static void test_wrong_state_transitions(void)
{
    exe_engine_t engine;
    exe_callbacks_t cb = make_callbacks();
    uint32_t id;

    printf("test_wrong_state_transitions:\n");

    reset_mocks();
    exe_init(&engine, &cb);
    memset(FAKE_SIG, 0x66, 64);

    /* Create outbound */
    exe_send(&engine, FRAME_SEND, 5, "did:key:zAlice",
             FAKE_SIG, "did:key:zBob", 100, &id);

    /* Try acknowledge on an outbound (wrong state) */
    int rc = exe_acknowledge(&engine, id, FRAME_ACK, 5,
                             "did:key:zAlice", FAKE_SIG, 100, 0);
    ASSERT(rc == EXE_ERR_STATE, "can't acknowledge an outbound");

    /* Create inbound */
    uint32_t id2;
    exe_receive(&engine, FRAME_SEND, 5, "did:key:zCharlie",
                FAKE_SIG, 200, 0, &id2);

    /* Try complete_outbound on an inbound (wrong state) */
    rc = exe_complete_outbound(&engine, id2, FRAME_ACK, 5,
                               "did:key:zCharlie", FAKE_SIG, 200, 1);
    ASSERT(rc == EXE_ERR_STATE, "can't complete_outbound on inbound");
}

/* ======================================================================== */

int main(void)
{
    printf("=== Exchange Engine unit tests ===\n\n");

    test_init();
    test_send_creates_outbound();
    test_full_outbound_exchange();
    test_full_inbound_exchange();
    test_conservation_failure();
    test_conservation_outbound_failure();
    test_signature_verification_failure();
    test_cancel();
    test_post_failure();
    test_receive_matches_outbound();
    test_multiple_exchanges();
    test_exchange_table_full();
    test_null_checks();
    test_wrong_state_transitions();

    printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? EXIT_SUCCESS : EXIT_FAILURE;
}
