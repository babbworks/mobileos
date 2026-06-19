/*
 * test_sharedb.c — Unit tests for telux-sharedb
 *
 * Tests queue management, carrier selection, transport dispatch,
 * power mode awareness, retry/failure handling, mock loopback.
 */

#include "sharedb.h"
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
 * MOCK CARRIERS
 * ======================================================================== */

static int mock_sms_sent = 0;
static int mock_ip_sent = 0;
static int mock_sms_fail = 0;
static int mock_ip_available = 1;
static int mock_sms_available = 1;

static int mock_sms_send(const uint8_t *payload, size_t len,
                         const char *endpoint, void *ctx)
{
    (void)payload; (void)len; (void)endpoint; (void)ctx;
    if (mock_sms_fail) { return -1; }
    mock_sms_sent++;
    return 0;
}

static int mock_sms_avail(void *ctx) { (void)ctx; return mock_sms_available; }

static int mock_ip_send(const uint8_t *payload, size_t len,
                        const char *endpoint, void *ctx)
{
    (void)payload; (void)len; (void)endpoint; (void)ctx;
    mock_ip_sent++;
    return 0;
}

static int mock_ip_avail(void *ctx) { (void)ctx; return mock_ip_available; }

static void reset_mocks(void)
{
    mock_sms_sent = 0;
    mock_ip_sent = 0;
    mock_sms_fail = 0;
    mock_ip_available = 1;
    mock_sms_available = 1;
}

static sdb_carrier_t make_sms_carrier(void)
{
    sdb_carrier_t c;
    memset(&c, 0, sizeof(c));
    c.carrier_type = SDB_CARRIER_SMS;
    c.send = mock_sms_send;
    c.available = mock_sms_avail;
    c.ctx = NULL;
    return c;
}

static sdb_carrier_t make_ip_carrier(void)
{
    sdb_carrier_t c;
    memset(&c, 0, sizeof(c));
    c.carrier_type = SDB_CARRIER_IP;
    c.send = mock_ip_send;
    c.available = mock_ip_avail;
    c.ctx = NULL;
    return c;
}

static sdb_peer_t make_sms_peer(void)
{
    sdb_peer_t p;
    memset(&p, 0, sizeof(p));
    strncpy(p.did, "did:key:zBob", SDB_DID_MAX - 1);
    p.has_sms = 1;
    strncpy(p.sms_endpoint, "+260971234567", SDB_ENDPOINT_MAX - 1);
    return p;
}

static sdb_peer_t make_ip_peer(void)
{
    sdb_peer_t p;
    memset(&p, 0, sizeof(p));
    strncpy(p.did, "did:key:zCharlie", SDB_DID_MAX - 1);
    p.has_ip = 1;
    strncpy(p.ip_endpoint, "https://relay.babb.tel/z6Mk...", SDB_ENDPOINT_MAX - 1);
    p.has_sms = 1;
    strncpy(p.sms_endpoint, "+260972222222", SDB_ENDPOINT_MAX - 1);
    return p;
}

/* ======================================================================== */

static void test_init(void)
{
    sdb_state_t state;

    printf("test_init:\n");

    int rc = sdb_init(&state);
    ASSERT(rc == SDB_OK, "init OK");
    ASSERT(state.next_id == 1, "next_id = 1");
    ASSERT(state.carrier_count == 0, "no carriers");
    ASSERT(state.power_mode == SDB_MODE_FULL, "default FULL mode");
    ASSERT(sdb_get_queue_depth(&state) == 0, "empty queue");
    ASSERT(sdb_init(NULL) == SDB_ERR_NULL, "NULL check");
}

static void test_add_carriers(void)
{
    sdb_state_t state;
    sdb_carrier_t sms, ip;

    printf("test_add_carriers:\n");

    sdb_init(&state);
    sms = make_sms_carrier();
    ip = make_ip_carrier();

    int rc = sdb_add_carrier(&state, &sms);
    ASSERT(rc == SDB_OK, "add SMS OK");
    ASSERT(state.carrier_count == 1, "1 carrier");

    rc = sdb_add_carrier(&state, &ip);
    ASSERT(rc == SDB_OK, "add IP OK");
    ASSERT(state.carrier_count == 2, "2 carriers");
}

static void test_enqueue_basic(void)
{
    sdb_state_t state;
    uint32_t id;
    uint8_t entry_state;
    uint8_t frame[] = { 0x80, 0x01, 0xF4, 0x00, 0x04 };
    sdb_peer_t peer = make_sms_peer();

    printf("test_enqueue_basic:\n");

    sdb_init(&state);

    int rc = sdb_enqueue(&state, frame, 5, &peer, &id);
    ASSERT(rc == SDB_OK, "enqueue OK");
    ASSERT(id == 1, "first entry id=1");
    ASSERT(sdb_get_queue_depth(&state) == 1, "queue depth=1");
    ASSERT(state.enqueued == 1, "enqueued count");

    rc = sdb_get_entry_state(&state, id, &entry_state);
    ASSERT(rc == SDB_OK, "get state OK");
    ASSERT(entry_state == SDB_ENTRY_QUEUED, "state=QUEUED");
}

static void test_process_sends_via_sms(void)
{
    sdb_state_t state;
    sdb_carrier_t sms = make_sms_carrier();
    uint32_t id;
    uint8_t entry_state;
    uint8_t frame[] = { 0x01, 0x02, 0x03 };
    sdb_peer_t peer = make_sms_peer();

    printf("test_process_sends_via_sms:\n");

    reset_mocks();
    sdb_init(&state);
    sdb_add_carrier(&state, &sms);

    sdb_enqueue(&state, frame, 3, &peer, &id);

    int sent = sdb_process_queue(&state);
    ASSERT(sent == 1, "1 sent");
    ASSERT(mock_sms_sent == 1, "SMS carrier called");
    ASSERT(state.sent == 1, "sent stat");

    sdb_get_entry_state(&state, id, &entry_state);
    ASSERT(entry_state == SDB_ENTRY_SENT, "state=SENT");
    ASSERT(sdb_get_queue_depth(&state) == 0, "queue empty after send");
}

static void test_ip_preferred_in_full_mode(void)
{
    sdb_state_t state;
    sdb_carrier_t sms = make_sms_carrier();
    sdb_carrier_t ip = make_ip_carrier();
    sdb_peer_t peer = make_ip_peer(); /* has both IP and SMS */
    uint32_t id;

    printf("test_ip_preferred_in_full_mode:\n");

    reset_mocks();
    sdb_init(&state);
    sdb_add_carrier(&state, &sms);
    sdb_add_carrier(&state, &ip);
    sdb_set_power_mode(&state, SDB_MODE_FULL);

    sdb_enqueue(&state, (uint8_t[]){0xAA}, 1, &peer, &id);
    sdb_process_queue(&state);

    ASSERT(mock_ip_sent == 1, "IP used in FULL mode");
    ASSERT(mock_sms_sent == 0, "SMS not used when IP available");
}

static void test_sms_preferred_in_conservation(void)
{
    sdb_state_t state;
    sdb_carrier_t sms = make_sms_carrier();
    sdb_carrier_t ip = make_ip_carrier();
    sdb_peer_t peer = make_ip_peer();
    uint32_t id;

    printf("test_sms_preferred_in_conservation:\n");

    reset_mocks();
    sdb_init(&state);
    sdb_add_carrier(&state, &sms);
    sdb_add_carrier(&state, &ip);
    sdb_set_power_mode(&state, SDB_MODE_CONS);

    sdb_enqueue(&state, (uint8_t[]){0xBB}, 1, &peer, &id);
    sdb_process_queue(&state);

    ASSERT(mock_sms_sent == 1, "SMS preferred in CONS mode");
    ASSERT(mock_ip_sent == 0, "IP deprioritized in CONS");
}

static void test_sms_preferred_in_emergency(void)
{
    sdb_state_t state;
    sdb_carrier_t sms = make_sms_carrier();
    sdb_carrier_t ip = make_ip_carrier();
    sdb_peer_t peer = make_ip_peer();
    uint32_t id;

    printf("test_sms_preferred_in_emergency:\n");

    reset_mocks();
    sdb_init(&state);
    sdb_add_carrier(&state, &sms);
    sdb_add_carrier(&state, &ip);
    sdb_set_power_mode(&state, SDB_MODE_EMRG);

    sdb_enqueue(&state, (uint8_t[]){0xCC}, 1, &peer, &id);
    sdb_process_queue(&state);

    ASSERT(mock_sms_sent == 1, "SMS in EMRG");
    ASSERT(mock_ip_sent == 0, "IP suspended in EMRG");
}

static void test_fallback_when_ip_unavailable(void)
{
    sdb_state_t state;
    sdb_carrier_t sms = make_sms_carrier();
    sdb_carrier_t ip = make_ip_carrier();
    sdb_peer_t peer = make_ip_peer();
    uint32_t id;

    printf("test_fallback_when_ip_unavailable:\n");

    reset_mocks();
    mock_ip_available = 0; /* IP down */
    sdb_init(&state);
    sdb_add_carrier(&state, &sms);
    sdb_add_carrier(&state, &ip);
    sdb_set_power_mode(&state, SDB_MODE_FULL);

    sdb_enqueue(&state, (uint8_t[]){0xDD}, 1, &peer, &id);
    sdb_process_queue(&state);

    ASSERT(mock_sms_sent == 1, "falls back to SMS when IP unavailable");
    ASSERT(mock_ip_sent == 0, "IP not used");
}

static void test_retry_on_failure(void)
{
    sdb_state_t state;
    sdb_carrier_t sms = make_sms_carrier();
    sdb_peer_t peer = make_sms_peer();
    uint32_t id;
    uint8_t entry_state;

    printf("test_retry_on_failure:\n");

    reset_mocks();
    mock_sms_fail = 1;
    sdb_init(&state);
    sdb_add_carrier(&state, &sms);

    sdb_enqueue(&state, (uint8_t[]){0xEE}, 1, &peer, &id);

    /* First attempt — fails, retries < 3, stays queued */
    sdb_process_queue(&state);
    sdb_get_entry_state(&state, id, &entry_state);
    ASSERT(entry_state == SDB_ENTRY_QUEUED, "retry 1: stays queued");

    /* Second attempt */
    sdb_process_queue(&state);
    sdb_get_entry_state(&state, id, &entry_state);
    ASSERT(entry_state == SDB_ENTRY_QUEUED, "retry 2: stays queued");

    /* Third attempt — retries exhausted → FAILED */
    sdb_process_queue(&state);
    sdb_get_entry_state(&state, id, &entry_state);
    ASSERT(entry_state == SDB_ENTRY_FAILED, "retry 3: FAILED");
    ASSERT(state.failed == 1, "failed count");
}

static void test_no_carrier_stays_queued(void)
{
    sdb_state_t state;
    sdb_peer_t peer;
    uint32_t id;
    uint8_t entry_state;

    printf("test_no_carrier_stays_queued:\n");

    reset_mocks();
    sdb_init(&state);
    /* No carriers registered */

    memset(&peer, 0, sizeof(peer));
    strncpy(peer.did, "did:key:zNobody", SDB_DID_MAX - 1);
    peer.has_sms = 1;
    strncpy(peer.sms_endpoint, "+123", SDB_ENDPOINT_MAX - 1);

    sdb_enqueue(&state, (uint8_t[]){0xFF}, 1, &peer, &id);
    sdb_process_queue(&state);

    sdb_get_entry_state(&state, id, &entry_state);
    ASSERT(entry_state == SDB_ENTRY_QUEUED, "stays queued when no carrier");
}

static void test_mark_acked(void)
{
    sdb_state_t state;
    sdb_carrier_t sms = make_sms_carrier();
    sdb_peer_t peer = make_sms_peer();
    uint32_t id;
    uint8_t entry_state;

    printf("test_mark_acked:\n");

    reset_mocks();
    sdb_init(&state);
    sdb_add_carrier(&state, &sms);

    sdb_enqueue(&state, (uint8_t[]){0x11}, 1, &peer, &id);
    sdb_process_queue(&state);

    int rc = sdb_mark_acked(&state, id);
    ASSERT(rc == SDB_OK, "mark acked OK");
    sdb_get_entry_state(&state, id, &entry_state);
    ASSERT(entry_state == SDB_ENTRY_ACKED, "state=ACKED");
}

static void test_select_carrier_api(void)
{
    sdb_state_t state;
    sdb_carrier_t sms = make_sms_carrier();
    sdb_carrier_t ip = make_ip_carrier();

    printf("test_select_carrier_api:\n");

    reset_mocks();
    sdb_init(&state);
    sdb_add_carrier(&state, &sms);
    sdb_add_carrier(&state, &ip);

    sdb_peer_t sms_only = make_sms_peer();
    sdb_peer_t ip_peer = make_ip_peer();

    /* SMS-only peer → SMS */
    ASSERT(sdb_select_carrier(&state, &sms_only) == SDB_CARRIER_SMS, "SMS peer → SMS");

    /* IP peer in FULL → IP */
    sdb_set_power_mode(&state, SDB_MODE_FULL);
    ASSERT(sdb_select_carrier(&state, &ip_peer) == SDB_CARRIER_IP, "IP peer in FULL → IP");

    /* IP peer in CONS → SMS */
    sdb_set_power_mode(&state, SDB_MODE_CONS);
    ASSERT(sdb_select_carrier(&state, &ip_peer) == SDB_CARRIER_SMS, "IP peer in CONS → SMS");
}

static void test_multiple_queued(void)
{
    sdb_state_t state;
    sdb_carrier_t sms = make_sms_carrier();
    sdb_peer_t peer = make_sms_peer();
    uint32_t ids[5];
    size_t i;

    printf("test_multiple_queued:\n");

    reset_mocks();
    sdb_init(&state);
    sdb_add_carrier(&state, &sms);

    for (i = 0; i < 5; i++) {
        uint8_t frame = (uint8_t)i;
        sdb_enqueue(&state, &frame, 1, &peer, &ids[i]);
    }
    ASSERT(sdb_get_queue_depth(&state) == 5, "5 queued");

    int sent = sdb_process_queue(&state);
    ASSERT(sent == 5, "all 5 sent");
    ASSERT(mock_sms_sent == 5, "5 SMS calls");
    ASSERT(sdb_get_queue_depth(&state) == 0, "queue drained");
}

static void test_null_checks(void)
{
    sdb_state_t state;
    uint32_t id;
    uint8_t s;
    sdb_peer_t peer = make_sms_peer();

    printf("test_null_checks:\n");

    sdb_init(&state);
    ASSERT(sdb_enqueue(NULL, (uint8_t[]){1}, 1, &peer, &id) == SDB_ERR_NULL, "enqueue NULL state");
    ASSERT(sdb_enqueue(&state, NULL, 1, &peer, &id) == SDB_ERR_NULL, "enqueue NULL frame");
    ASSERT(sdb_enqueue(&state, (uint8_t[]){1}, 1, NULL, &id) == SDB_ERR_NULL, "enqueue NULL peer");
    ASSERT(sdb_process_queue(NULL) == SDB_ERR_NULL, "process NULL");
    ASSERT(sdb_get_entry_state(NULL, 1, &s) == SDB_ERR_NULL, "get_state NULL");
    ASSERT(sdb_get_entry_state(&state, 999, &s) == SDB_ERR_NOT_FOUND, "not found");
    ASSERT(sdb_mark_acked(NULL, 1) == SDB_ERR_NULL, "mark NULL");
    ASSERT(sdb_add_carrier(NULL, NULL) == SDB_ERR_NULL, "add NULL");
    ASSERT(sdb_set_power_mode(NULL, 0) == SDB_ERR_NULL, "set_mode NULL");
}

/* ======================================================================== */

int main(void)
{
    printf("=== telux-sharedb unit tests ===\n\n");

    test_init();
    test_add_carriers();
    test_enqueue_basic();
    test_process_sends_via_sms();
    test_ip_preferred_in_full_mode();
    test_sms_preferred_in_conservation();
    test_sms_preferred_in_emergency();
    test_fallback_when_ip_unavailable();
    test_retry_on_failure();
    test_no_carrier_stays_queued();
    test_mark_acked();
    test_select_carrier_api();
    test_multiple_queued();
    test_null_checks();

    printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? EXIT_SUCCESS : EXIT_FAILURE;
}
