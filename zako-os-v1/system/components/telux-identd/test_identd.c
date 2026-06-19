/*
 * test_identd.c — Unit tests for telux-identd (store + daemon)
 *
 * Tests key generation, DID management, signing service, identity lock,
 * capability grants/revocation/cascade, and bus-connected daemon operation.
 */

#include "identd_store.h"
#include "identd_daemon.h"
#include "../libzako-bus/zako_bus.h"
#include "../libzako-sign/zako_sign.h"
#include "../libzako-did/zako_did.h"
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

#define TEST_SOCKET_PATH "/tmp/zako_identd_test.sock"

/* ========================================================================
 * STORE TESTS
 * ======================================================================== */

static void test_store_open_close(void)
{
    ids_store_t store;
    int rc;

    printf("test_store_open_close:\n");

    rc = ids_open(&store, ":memory:");
    ASSERT(rc == IDS_OK, "open OK");
    ASSERT(store.db != NULL, "db valid");
    ASSERT(store.sovereign_key_id == -1, "no sovereign yet");

    ids_close(&store);
    ASSERT(store.db == NULL, "closed");

    ASSERT(ids_open(NULL, ":memory:") == IDS_ERR_NULL, "NULL store");
    ASSERT(ids_open(&store, NULL) == IDS_ERR_NULL, "NULL path");
}

static void test_sovereign_key(void)
{
    ids_store_t store;
    ids_key_t key, retrieved;
    int rc;

    printf("test_sovereign_key:\n");

    ids_open(&store, ":memory:");

    /* Generate sovereign */
    rc = ids_generate_sovereign(&store, &key);
    ASSERT(rc == IDS_OK, "generate sovereign OK");
    ASSERT(key.is_sovereign == 1, "is_sovereign flag");
    ASSERT(strcmp(key.label, "sovereign") == 0, "label = sovereign");
    ASSERT(strlen(key.did) > 50, "DID is a valid-looking string");
    ASSERT(zako_did_is_valid(key.did) == 1, "DID passes validation");

    /* Second generate should fail */
    ids_key_t dup;
    rc = ids_generate_sovereign(&store, &dup);
    ASSERT(rc == IDS_ERR_EXISTS, "duplicate sovereign rejected");

    /* Retrieve sovereign */
    rc = ids_get_sovereign(&store, &retrieved);
    ASSERT(rc == IDS_OK, "get_sovereign OK");
    ASSERT(strcmp(retrieved.did, key.did) == 0, "DIDs match");
    ASSERT(memcmp(retrieved.pubkey, key.pubkey, ZAKO_SIGN_PUBKEY_LEN) == 0, "pubkeys match");

    zako_sign_seckey_zero(key.seckey);
    zako_sign_seckey_zero(retrieved.seckey);
    ids_close(&store);
}

static void test_derived_keys(void)
{
    ids_store_t store;
    ids_key_t k1, k2, found;
    int rc;

    printf("test_derived_keys:\n");

    ids_open(&store, ":memory:");
    ids_key_t sov;
    ids_generate_sovereign(&store, &sov);

    /* Generate derived keys */
    rc = ids_generate_key(&store, "island:work", &k1);
    ASSERT(rc == IDS_OK, "key1 generated");
    ASSERT(k1.is_sovereign == 0, "not sovereign");
    ASSERT(strcmp(k1.label, "island:work") == 0, "label matches");

    rc = ids_generate_key(&store, "island:personal", &k2);
    ASSERT(rc == IDS_OK, "key2 generated");
    ASSERT(strcmp(k1.did, k2.did) != 0, "different keys have different DIDs");

    /* Look up by DID */
    rc = ids_get_key_by_did(&store, k1.did, &found);
    ASSERT(rc == IDS_OK, "find by DID");
    ASSERT(strcmp(found.label, "island:work") == 0, "found correct key");

    /* Look up by label */
    rc = ids_get_key_by_label(&store, "island:personal", &found);
    ASSERT(rc == IDS_OK, "find by label");
    ASSERT(strcmp(found.did, k2.did) == 0, "found correct DID");

    /* Not found */
    rc = ids_get_key_by_did(&store, "did:key:zNOTEXIST", &found);
    ASSERT(rc == IDS_ERR_NOT_FOUND, "missing DID returns NOT_FOUND");

    /* Key count */
    ASSERT(ids_get_key_count(&store) == 3, "3 keys total (sov + 2 derived)");

    zako_sign_seckey_zero(sov.seckey);
    zako_sign_seckey_zero(k1.seckey);
    zako_sign_seckey_zero(k2.seckey);
    ids_close(&store);
}

static void test_deterministic_keygen(void)
{
    ids_store_t store;
    ids_key_t k1, k2;
    uint8_t seed[ZAKO_SIGN_SEED_LEN];
    int rc;

    printf("test_deterministic_keygen:\n");

    memset(seed, 0x42, ZAKO_SIGN_SEED_LEN);

    ids_open(&store, ":memory:");
    ids_key_t sov;
    ids_generate_sovereign(&store, &sov);

    rc = ids_generate_key_from_seed(&store, "test1", seed, &k1);
    ASSERT(rc == IDS_OK, "seeded keygen OK");

    /* Same seed in different store = same DID */
    ids_store_t store2;
    ids_open(&store2, ":memory:");
    ids_key_t sov2;
    ids_generate_sovereign(&store2, &sov2);

    rc = ids_generate_key_from_seed(&store2, "test1", seed, &k2);
    ASSERT(rc == IDS_OK, "seeded keygen 2 OK");
    ASSERT(strcmp(k1.did, k2.did) == 0, "same seed = same DID");

    zako_sign_seckey_zero(sov.seckey);
    zako_sign_seckey_zero(sov2.seckey);
    zako_sign_seckey_zero(k1.seckey);
    zako_sign_seckey_zero(k2.seckey);
    ids_close(&store);
    ids_close(&store2);
}

static void test_signing_service(void)
{
    ids_store_t store;
    ids_key_t sov;
    uint8_t sig[ZAKO_SIGN_SIG_LEN];
    const uint8_t msg[] = "Hello ZAKO";
    int rc;

    printf("test_signing_service:\n");

    ids_open(&store, ":memory:");
    ids_generate_sovereign(&store, &sov);

    /* Sign with sovereign */
    rc = ids_sign(&store, sov.did, msg, sizeof(msg) - 1, sig);
    ASSERT(rc == IDS_OK, "sign OK");

    /* Verify with DID (self-resolving) */
    rc = ids_verify(sov.did, msg, sizeof(msg) - 1, sig);
    ASSERT(rc == IDS_OK, "verify OK");

    /* Tamper with message → verify fails */
    uint8_t bad_msg[] = "Hello XAKO";
    rc = ids_verify(sov.did, bad_msg, sizeof(bad_msg) - 1, sig);
    ASSERT(rc == IDS_ERR_VERIFY, "tampered msg fails");

    /* Tamper with sig → verify fails */
    uint8_t bad_sig[ZAKO_SIGN_SIG_LEN];
    memcpy(bad_sig, sig, ZAKO_SIGN_SIG_LEN);
    bad_sig[0] ^= 0x01u;
    rc = ids_verify(sov.did, msg, sizeof(msg) - 1, bad_sig);
    ASSERT(rc == IDS_ERR_VERIFY, "tampered sig fails");

    zako_sign_seckey_zero(sov.seckey);
    ids_close(&store);
}

static void test_identity_lock(void)
{
    ids_store_t store;
    ids_key_t sov;
    uint8_t sig[ZAKO_SIGN_SIG_LEN];
    const uint8_t msg[] = "test";
    int rc;

    printf("test_identity_lock:\n");

    ids_open(&store, ":memory:");
    ids_generate_sovereign(&store, &sov);

    ASSERT(ids_is_locked(&store) == 0, "initially unlocked");

    /* Lock */
    ids_lock(&store);
    ASSERT(ids_is_locked(&store) == 1, "now locked");

    /* Signing should fail */
    rc = ids_sign(&store, sov.did, msg, sizeof(msg) - 1, sig);
    ASSERT(rc == IDS_ERR_LOCKED, "sign fails when locked");

    /* Unlock */
    ids_unlock(&store);
    ASSERT(ids_is_locked(&store) == 0, "unlocked again");

    /* Signing works again */
    rc = ids_sign(&store, sov.did, msg, sizeof(msg) - 1, sig);
    ASSERT(rc == IDS_OK, "sign OK after unlock");

    zako_sign_seckey_zero(sov.seckey);
    ids_close(&store);
}

static void test_capability_grant(void)
{
    ids_store_t store;
    ids_key_t sov, k1;
    ids_cap_t cap;
    int rc;

    printf("test_capability_grant:\n");

    ids_open(&store, ":memory:");
    ids_generate_sovereign(&store, &sov);
    ids_generate_key(&store, "peer", &k1);

    /* Sovereign grants to peer */
    rc = ids_grant(&store, sov.did, k1.did, "ledger:write", &cap);
    ASSERT(rc == IDS_OK, "grant OK");
    ASSERT(cap.depth == 0, "sovereign grants at depth 0");
    ASSERT(strcmp(cap.capability, "ledger:write") == 0, "capability stored");
    ASSERT(strcmp(cap.grantee_did, k1.did) == 0, "grantee matches");

    /* Verify the grant signature */
    char payload[256];
    size_t payload_len = (size_t)snprintf(payload, sizeof(payload),
                                          "GRANT:%s:%s", "ledger:write", k1.did);
    rc = ids_verify(sov.did, (const uint8_t *)payload, payload_len, cap.sig);
    ASSERT(rc == IDS_OK, "grant signature valid");

    /* Check capability */
    rc = ids_check_capability(&store, k1.did, "ledger:write");
    ASSERT(rc == IDS_OK, "capability active");

    rc = ids_check_capability(&store, k1.did, "ledger:read");
    ASSERT(rc == IDS_ERR_NOT_FOUND, "ungranted capability not found");

    zako_sign_seckey_zero(sov.seckey);
    zako_sign_seckey_zero(k1.seckey);
    ids_close(&store);
}

static void test_capability_revoke(void)
{
    ids_store_t store;
    ids_key_t sov, k1;
    ids_cap_t cap;
    int rc;

    printf("test_capability_revoke:\n");

    ids_open(&store, ":memory:");
    ids_generate_sovereign(&store, &sov);
    ids_generate_key(&store, "worker", &k1);

    ids_grant(&store, sov.did, k1.did, "sign:records", &cap);
    ASSERT(ids_check_capability(&store, k1.did, "sign:records") == IDS_OK, "granted");

    /* Revoke */
    rc = ids_revoke(&store, cap.id);
    ASSERT(rc == IDS_OK, "revoke OK");

    rc = ids_check_capability(&store, k1.did, "sign:records");
    ASSERT(rc == IDS_ERR_REVOKED, "revoked capability returns REVOKED");

    zako_sign_seckey_zero(sov.seckey);
    zako_sign_seckey_zero(k1.seckey);
    ids_close(&store);
}

static void test_capability_cascade(void)
{
    ids_store_t store;
    ids_key_t sov, k1, k2;
    ids_cap_t cap1, cap2;
    int rc;

    printf("test_capability_cascade:\n");

    ids_open(&store, ":memory:");
    ids_generate_sovereign(&store, &sov);
    ids_generate_key(&store, "manager", &k1);
    ids_generate_key(&store, "worker", &k2);

    /* Sovereign → manager (depth 0) */
    rc = ids_grant(&store, sov.did, k1.did, "ledger:write", &cap1);
    ASSERT(rc == IDS_OK, "sov→manager grant");
    ASSERT(cap1.depth == 0, "depth 0");

    /* Manager → worker (depth 1) */
    rc = ids_grant(&store, k1.did, k2.did, "ledger:write", &cap2);
    ASSERT(rc == IDS_OK, "manager→worker grant");
    ASSERT(cap2.depth == 1, "depth 1");

    /* Both have capability */
    ASSERT(ids_check_capability(&store, k1.did, "ledger:write") == IDS_OK, "manager has cap");
    ASSERT(ids_check_capability(&store, k2.did, "ledger:write") == IDS_OK, "worker has cap");

    /* Revoke manager's grant → cascade to worker */
    rc = ids_revoke(&store, cap1.id);
    ASSERT(rc == IDS_OK, "revoke manager");

    /* Manager's cap is revoked */
    ASSERT(ids_check_capability(&store, k1.did, "ledger:write") == IDS_ERR_REVOKED, "manager revoked");

    /* Worker's cap cascaded (grantor=manager's DID is revoked) */
    ASSERT(ids_check_capability(&store, k2.did, "ledger:write") == IDS_ERR_REVOKED, "worker cascaded");

    zako_sign_seckey_zero(sov.seckey);
    zako_sign_seckey_zero(k1.seckey);
    zako_sign_seckey_zero(k2.seckey);
    ids_close(&store);
}

static void test_depth_limit(void)
{
    ids_store_t store;
    ids_key_t sov, keys[5];
    ids_cap_t cap;
    int rc;
    size_t i;

    printf("test_depth_limit:\n");

    ids_open(&store, ":memory:");
    ids_generate_sovereign(&store, &sov);

    /* Create a chain of keys */
    uint8_t seed[ZAKO_SIGN_SEED_LEN];
    for (i = 0; i < 5; i++) {
        char label[32];
        snprintf(label, sizeof(label), "key%zu", i);
        memset(seed, (int)(i + 1), ZAKO_SIGN_SEED_LEN);
        ids_generate_key_from_seed(&store, label, seed, &keys[i]);
    }

    /* Sov → key0 (depth 0) */
    rc = ids_grant(&store, sov.did, keys[0].did, "test", &cap);
    ASSERT(rc == IDS_OK, "depth 0 grant");

    /* key0 → key1 (depth 1) */
    rc = ids_grant(&store, keys[0].did, keys[1].did, "test", &cap);
    ASSERT(rc == IDS_OK, "depth 1 grant");

    /* key1 → key2 (depth 2) */
    rc = ids_grant(&store, keys[1].did, keys[2].did, "test", &cap);
    ASSERT(rc == IDS_OK, "depth 2 grant");

    /* key2 → key3 (depth 3) */
    rc = ids_grant(&store, keys[2].did, keys[3].did, "test", &cap);
    ASSERT(rc == IDS_OK, "depth 3 grant (max)");

    /* key3 → key4 (depth 4 — EXCEEDS MAX) */
    rc = ids_grant(&store, keys[3].did, keys[4].did, "test", &cap);
    ASSERT(rc == IDS_ERR_DEPTH, "depth 4 rejected");

    zako_sign_seckey_zero(sov.seckey);
    for (i = 0; i < 5; i++) { zako_sign_seckey_zero(keys[i].seckey); }
    ids_close(&store);
}

static void test_get_grants_for(void)
{
    ids_store_t store;
    ids_key_t sov, k1;
    ids_cap_t cap;
    ids_cap_t caps[8];
    size_t count;
    int rc;

    printf("test_get_grants_for:\n");

    ids_open(&store, ":memory:");
    ids_generate_sovereign(&store, &sov);
    ids_generate_key(&store, "agent", &k1);

    /* Grant 3 capabilities */
    ids_grant(&store, sov.did, k1.did, "ledger:write", &cap);
    ids_grant(&store, sov.did, k1.did, "sign:records", &cap);
    ids_grant(&store, sov.did, k1.did, "exchange:send", &cap);

    /* Query */
    rc = ids_get_grants_for(&store, k1.did, caps, 8, &count);
    ASSERT(rc == IDS_OK, "get_grants OK");
    ASSERT(count == 3, "3 active grants");

    /* Revoke one */
    ids_revoke(&store, caps[0].id);
    rc = ids_get_grants_for(&store, k1.did, caps, 8, &count);
    ASSERT(count == 2, "2 active after revoke");

    zako_sign_seckey_zero(sov.seckey);
    zako_sign_seckey_zero(k1.seckey);
    ids_close(&store);
}

/* ========================================================================
 * DAEMON TESTS (bus-connected)
 * ======================================================================== */

typedef struct {
    zbu_server_t  server;
    zbu_conn_t    sender;
    idd_daemon_t  daemon;
} test_env_t;

static int env_setup(test_env_t *env)
{
    int rc;

    zbu_server_init(&env->server);
    rc = zbu_server_start(&env->server, TEST_SOCKET_PATH);
    if (rc != ZBU_OK) { return -1; }

    idd_init(&env->daemon, TEST_SOCKET_PATH, ":memory:");
    rc = idd_start(&env->daemon);
    if (rc != IDD_OK) {
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

    /* Connect sender */
    zbu_client_init(&env->sender);
    rc = zbu_client_connect(&env->sender, TEST_SOCKET_PATH);
    if (rc != ZBU_OK) {
        idd_shutdown(&env->daemon);
        zbu_server_stop(&env->server);
        return -1;
    }
    usleep(10000);
    zbu_server_poll(&env->server, 100);

    return 0;
}

static void env_teardown(test_env_t *env)
{
    idd_shutdown(&env->daemon);
    zbu_client_disconnect(&env->sender);
    zbu_server_stop(&env->server);
    usleep(10000);
}

static int env_send_to_daemon(test_env_t *env, const uint8_t *data, size_t len)
{
    size_t i;
    for (i = 0; i < ZBU_MAX_CLIENTS; i++) {
        if (env->server.clients[i].state == ZBU_STATE_ACTIVE) {
            return zbu_server_send(&env->server, i, data, len);
        }
    }
    return -1;
}

static int env_poll_and_recv(test_env_t *env, zbu_frame_t *resp)
{
    usleep(10000);
    idd_poll(&env->daemon, 0);
    usleep(10000);
    zbu_server_poll(&env->server, 100);
    return zbu_server_recv(&env->server, resp);
}

static void test_daemon_get_sovereign(void)
{
    test_env_t env;
    zbu_frame_t resp;

    printf("test_daemon_get_sovereign:\n");

    if (env_setup(&env) != 0) { ASSERT(0, "setup failed"); return; }

    /* Send GET_SOVEREIGN request */
    uint8_t req[] = { IDD_OP_GET_SOVEREIGN };
    env_send_to_daemon(&env, req, 1);

    int rc = env_poll_and_recv(&env, &resp);
    ASSERT(rc == ZBU_OK, "got response");
    ASSERT(resp.data[0] == (IDD_OP_GET_SOVEREIGN | IDD_RESP_BIT), "correct opcode");
    ASSERT(resp.data[1] == IDD_STATUS_OK, "status OK");
    /* DID follows at resp.data[2..] */
    char *did = (char *)&resp.data[2];
    ASSERT(zako_did_is_valid(did) == 1, "returned DID is valid");

    env_teardown(&env);
}

static void test_daemon_sign_verify(void)
{
    test_env_t env;
    zbu_frame_t resp;

    printf("test_daemon_sign_verify:\n");

    if (env_setup(&env) != 0) { ASSERT(0, "setup failed"); return; }

    /* First get sovereign DID */
    uint8_t get_req[] = { IDD_OP_GET_SOVEREIGN };
    env_send_to_daemon(&env, get_req, 1);
    env_poll_and_recv(&env, &resp);
    char did[ZAKO_DID_STR_MAX];
    strncpy(did, (const char *)&resp.data[2], ZAKO_DID_STR_MAX - 1);
    did[ZAKO_DID_STR_MAX - 1] = '\0';
    uint8_t did_len = (uint8_t)strlen(did);

    /* Build SIGN request: [opcode][did_len][did][msg_len_hi][msg_len_lo][msg] */
    const char *msg = "Hello";
    uint16_t msg_len = 5;
    uint8_t sign_req[256];
    size_t off = 0;
    sign_req[off++] = IDD_OP_SIGN;
    sign_req[off++] = did_len;
    memcpy(sign_req + off, did, did_len);
    off += did_len;
    sign_req[off++] = (uint8_t)(msg_len >> 8);
    sign_req[off++] = (uint8_t)(msg_len & 0xFF);
    memcpy(sign_req + off, msg, msg_len);
    off += msg_len;

    env_send_to_daemon(&env, sign_req, off);
    int rc = env_poll_and_recv(&env, &resp);
    ASSERT(rc == ZBU_OK, "sign response received");
    ASSERT(resp.data[0] == (IDD_OP_SIGN | IDD_RESP_BIT), "sign opcode");
    ASSERT(resp.data[1] == IDD_STATUS_OK, "sign OK");
    /* sig at resp.data[2..66] */
    uint8_t sig[ZAKO_SIGN_SIG_LEN];
    memcpy(sig, &resp.data[2], ZAKO_SIGN_SIG_LEN);

    /* Now VERIFY: [opcode][did_len][did][sig(64)][msg_len_hi][msg_len_lo][msg] */
    uint8_t verify_req[256];
    off = 0;
    verify_req[off++] = IDD_OP_VERIFY;
    verify_req[off++] = did_len;
    memcpy(verify_req + off, did, did_len);
    off += did_len;
    memcpy(verify_req + off, sig, ZAKO_SIGN_SIG_LEN);
    off += ZAKO_SIGN_SIG_LEN;
    verify_req[off++] = (uint8_t)(msg_len >> 8);
    verify_req[off++] = (uint8_t)(msg_len & 0xFF);
    memcpy(verify_req + off, msg, msg_len);
    off += msg_len;

    env_send_to_daemon(&env, verify_req, off);
    rc = env_poll_and_recv(&env, &resp);
    ASSERT(rc == ZBU_OK, "verify response received");
    ASSERT(resp.data[0] == (IDD_OP_VERIFY | IDD_RESP_BIT), "verify opcode");
    ASSERT(resp.data[1] == IDD_STATUS_OK, "verify OK");

    env_teardown(&env);
}

static void test_daemon_lock_unlock(void)
{
    test_env_t env;
    zbu_frame_t resp;

    printf("test_daemon_lock_unlock:\n");

    if (env_setup(&env) != 0) { ASSERT(0, "setup failed"); return; }

    /* Lock */
    uint8_t lock_req[] = { IDD_OP_LOCK };
    env_send_to_daemon(&env, lock_req, 1);
    env_poll_and_recv(&env, &resp);
    ASSERT(resp.data[1] == IDD_STATUS_OK, "lock OK");

    /* Status should report locked */
    uint8_t status_req[] = { IDD_OP_STATUS };
    env_send_to_daemon(&env, status_req, 1);
    env_poll_and_recv(&env, &resp);
    ASSERT(resp.data[1] == IDD_STATUS_OK, "status OK");
    ASSERT(resp.data[2] == 1, "locked = 1");

    /* Try signing while locked → should fail */
    /* Get sovereign DID first */
    uint8_t get_req[] = { IDD_OP_GET_SOVEREIGN };
    env_send_to_daemon(&env, get_req, 1);
    env_poll_and_recv(&env, &resp);
    char did[ZAKO_DID_STR_MAX];
    strncpy(did, (const char *)&resp.data[2], ZAKO_DID_STR_MAX - 1);
    uint8_t did_len = (uint8_t)strlen(did);

    uint8_t sign_req[128];
    size_t off = 0;
    sign_req[off++] = IDD_OP_SIGN;
    sign_req[off++] = did_len;
    memcpy(sign_req + off, did, did_len);
    off += did_len;
    sign_req[off++] = 0;
    sign_req[off++] = 4;
    memcpy(sign_req + off, "test", 4);
    off += 4;

    env_send_to_daemon(&env, sign_req, off);
    env_poll_and_recv(&env, &resp);
    ASSERT(resp.data[1] == IDD_STATUS_LOCKED, "sign rejected when locked");

    /* Unlock */
    uint8_t unlock_req[] = { IDD_OP_UNLOCK };
    env_send_to_daemon(&env, unlock_req, 1);
    env_poll_and_recv(&env, &resp);
    ASSERT(resp.data[1] == IDD_STATUS_OK, "unlock OK");

    /* Sign should work again */
    env_send_to_daemon(&env, sign_req, off);
    env_poll_and_recv(&env, &resp);
    ASSERT(resp.data[1] == IDD_STATUS_OK, "sign OK after unlock");

    env_teardown(&env);
}

static void test_daemon_keygen(void)
{
    test_env_t env;
    zbu_frame_t resp;

    printf("test_daemon_keygen:\n");

    if (env_setup(&env) != 0) { ASSERT(0, "setup failed"); return; }

    /* KEYGEN: [opcode][label_len][label] */
    const char *label = "island:test";
    uint8_t req[64];
    size_t off = 0;
    req[off++] = IDD_OP_KEYGEN;
    req[off++] = (uint8_t)strlen(label);
    memcpy(req + off, label, strlen(label));
    off += strlen(label);

    env_send_to_daemon(&env, req, off);
    int rc = env_poll_and_recv(&env, &resp);
    ASSERT(rc == ZBU_OK, "keygen response");
    ASSERT(resp.data[0] == (IDD_OP_KEYGEN | IDD_RESP_BIT), "keygen opcode");
    ASSERT(resp.data[1] == IDD_STATUS_OK, "keygen OK");

    /* DID in response */
    char *new_did = (char *)&resp.data[2];
    ASSERT(zako_did_is_valid(new_did) == 1, "generated DID is valid");

    ASSERT(env.daemon.keygen_count == 1, "keygen count = 1");

    env_teardown(&env);
}

static void test_null_checks(void)
{
    printf("test_null_checks:\n");

    ASSERT(idd_init(NULL, "/tmp/x", "/tmp/y") == IDD_ERR_NULL, "init NULL");
    ASSERT(idd_poll(NULL, 0) == IDD_ERR_NULL, "poll NULL");

    idd_daemon_t d;
    idd_init(&d, TEST_SOCKET_PATH, ":memory:");
    d.running = 1;
    idd_stop(&d);
    ASSERT(d.running == 0, "stop works");
    ASSERT(idd_poll(&d, 0) == IDD_ERR_SHUTDOWN, "poll after stop");
}

/* ======================================================================== */

int main(void)
{
    printf("=== telux-identd tests ===\n\n");

    /* Store tests */
    test_store_open_close();
    test_sovereign_key();
    test_derived_keys();
    test_deterministic_keygen();
    test_signing_service();
    test_identity_lock();
    test_capability_grant();
    test_capability_revoke();
    test_capability_cascade();
    test_depth_limit();
    test_get_grants_for();

    /* Daemon tests */
    test_daemon_get_sovereign();
    test_daemon_sign_verify();
    test_daemon_lock_unlock();
    test_daemon_keygen();
    test_null_checks();

    printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? EXIT_SUCCESS : EXIT_FAILURE;
}
