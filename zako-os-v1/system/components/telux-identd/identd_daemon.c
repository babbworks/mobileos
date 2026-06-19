/*
 * identd_daemon.c — telux-identd Daemon Shell Implementation
 *
 * Event loop handling identity/signing requests from the system bus.
 *
 * Request format:  [opcode(1)][payload...]
 * Response format: [opcode|0x80(1)][status(1)][response_payload...]
 *
 * SIGN request payload:
 *   [did_len(1)][did_str(did_len)][msg_len_hi(1)][msg_len_lo(1)][msg(msg_len)]
 * SIGN response payload:
 *   [status(1)][sig(64)]
 *
 * VERIFY request payload:
 *   [did_len(1)][did_str(did_len)][sig(64)][msg_len_hi(1)][msg_len_lo(1)][msg(msg_len)]
 * VERIFY response:
 *   [status(1)]
 *
 * KEYGEN request payload:
 *   [label_len(1)][label(label_len)]
 * KEYGEN response:
 *   [status(1)][did_str(null-terminated)]
 *
 * GET_SOVEREIGN response:
 *   [status(1)][did_str(null-terminated)]
 *
 * Copyright (c) 2026 Babb Works. Licensed under MIT.
 */

#include "identd_daemon.h"
#include <string.h>

/* ========================================================================
 * INTERNAL — RESPONSE BUILDERS
 * ======================================================================== */

static int send_response(idd_daemon_t *daemon, uint8_t opcode,
                         uint8_t status, const uint8_t *payload, size_t pay_len)
{
    uint8_t frame[ZBU_MAX_FRAME];
    size_t frame_len;

    frame[0] = opcode | IDD_RESP_BIT;
    frame[1] = status;
    frame_len = 2;

    if (payload != NULL && pay_len > 0) {
        if (frame_len + pay_len > ZBU_MAX_FRAME) { return IDD_ERR_FRAME; }
        memcpy(frame + frame_len, payload, pay_len);
        frame_len += pay_len;
    }

    return (zbu_client_send(&daemon->bus, frame, frame_len) == ZBU_OK)
           ? IDD_OK : IDD_ERR_BUS;
}

static int send_ok(idd_daemon_t *daemon, uint8_t opcode)
{
    return send_response(daemon, opcode, IDD_STATUS_OK, NULL, 0);
}

static int send_err(idd_daemon_t *daemon, uint8_t opcode, uint8_t status)
{
    return send_response(daemon, opcode, status, NULL, 0);
}

/* ========================================================================
 * INTERNAL — REQUEST HANDLERS
 * ======================================================================== */

static int handle_sign(idd_daemon_t *daemon, const uint8_t *data, size_t len)
{
    size_t offset = 0;
    uint8_t did_len;
    char did[ZAKO_DID_STR_MAX];
    uint16_t msg_len;
    uint8_t sig[ZAKO_SIGN_SIG_LEN];
    int rc;

    /* Parse: [did_len(1)][did(did_len)][msg_len_hi(1)][msg_len_lo(1)][msg(msg_len)] */
    if (len < 4) { return send_err(daemon, IDD_OP_SIGN, IDD_STATUS_ERR); }

    did_len = data[offset++];
    if (did_len == 0 || did_len >= ZAKO_DID_STR_MAX || offset + did_len + 2 > len) {
        return send_err(daemon, IDD_OP_SIGN, IDD_STATUS_ERR);
    }
    memcpy(did, data + offset, did_len);
    did[did_len] = '\0';
    offset += did_len;

    msg_len = (uint16_t)((data[offset] << 8u) | data[offset + 1]);
    offset += 2;

    if (msg_len == 0 || offset + msg_len > len) {
        return send_err(daemon, IDD_OP_SIGN, IDD_STATUS_ERR);
    }

    rc = ids_sign(&daemon->store, did, data + offset, msg_len, sig);

    if (rc == IDS_ERR_LOCKED) {
        return send_err(daemon, IDD_OP_SIGN, IDD_STATUS_LOCKED);
    }
    if (rc == IDS_ERR_NOT_FOUND) {
        return send_err(daemon, IDD_OP_SIGN, IDD_STATUS_NOT_FOUND);
    }
    if (rc != IDS_OK) {
        return send_err(daemon, IDD_OP_SIGN, IDD_STATUS_ERR);
    }

    daemon->sign_count++;
    return send_response(daemon, IDD_OP_SIGN, IDD_STATUS_OK, sig, ZAKO_SIGN_SIG_LEN);
}

static int handle_verify(idd_daemon_t *daemon, const uint8_t *data, size_t len)
{
    size_t offset = 0;
    uint8_t did_len;
    char did[ZAKO_DID_STR_MAX];
    uint8_t sig[ZAKO_SIGN_SIG_LEN];
    uint16_t msg_len;
    int rc;

    /* [did_len(1)][did(did_len)][sig(64)][msg_len_hi(1)][msg_len_lo(1)][msg(msg_len)] */
    if (len < 68) { return send_err(daemon, IDD_OP_VERIFY, IDD_STATUS_ERR); }

    did_len = data[offset++];
    if (did_len == 0 || did_len >= ZAKO_DID_STR_MAX ||
        offset + did_len + ZAKO_SIGN_SIG_LEN + 2 > len) {
        return send_err(daemon, IDD_OP_VERIFY, IDD_STATUS_ERR);
    }
    memcpy(did, data + offset, did_len);
    did[did_len] = '\0';
    offset += did_len;

    memcpy(sig, data + offset, ZAKO_SIGN_SIG_LEN);
    offset += ZAKO_SIGN_SIG_LEN;

    msg_len = (uint16_t)((data[offset] << 8u) | data[offset + 1]);
    offset += 2;

    if (msg_len == 0 || offset + msg_len > len) {
        return send_err(daemon, IDD_OP_VERIFY, IDD_STATUS_ERR);
    }

    rc = ids_verify(did, data + offset, msg_len, sig);
    daemon->verify_count++;

    if (rc == IDS_OK) {
        return send_ok(daemon, IDD_OP_VERIFY);
    } else {
        return send_err(daemon, IDD_OP_VERIFY, IDD_STATUS_ERR);
    }
}

static int handle_keygen(idd_daemon_t *daemon, const uint8_t *data, size_t len)
{
    uint8_t label_len;
    char label[IDS_LABEL_MAX];
    ids_key_t key;
    int rc;

    /* [label_len(1)][label(label_len)] */
    if (len < 2) { return send_err(daemon, IDD_OP_KEYGEN, IDD_STATUS_ERR); }

    label_len = data[0];
    if (label_len == 0 || label_len >= IDS_LABEL_MAX || 1 + label_len > len) {
        return send_err(daemon, IDD_OP_KEYGEN, IDD_STATUS_ERR);
    }
    memcpy(label, data + 1, label_len);
    label[label_len] = '\0';

    rc = ids_generate_key(&daemon->store, label, &key);
    if (rc != IDS_OK) {
        return send_err(daemon, IDD_OP_KEYGEN, IDD_STATUS_ERR);
    }

    /* Zero secret key from stack copy */
    zako_sign_seckey_zero(key.seckey);

    daemon->keygen_count++;

    /* Return the DID string */
    size_t did_len = strlen(key.did) + 1; /* include null terminator */
    return send_response(daemon, IDD_OP_KEYGEN, IDD_STATUS_OK,
                         (const uint8_t *)key.did, did_len);
}

static int handle_get_sovereign(idd_daemon_t *daemon)
{
    ids_key_t key;
    int rc;

    rc = ids_get_sovereign(&daemon->store, &key);
    if (rc == IDS_ERR_NOT_FOUND) {
        return send_err(daemon, IDD_OP_GET_SOVEREIGN, IDD_STATUS_NOT_FOUND);
    }
    if (rc != IDS_OK) {
        return send_err(daemon, IDD_OP_GET_SOVEREIGN, IDD_STATUS_ERR);
    }

    zako_sign_seckey_zero(key.seckey);

    size_t did_len = strlen(key.did) + 1;
    return send_response(daemon, IDD_OP_GET_SOVEREIGN, IDD_STATUS_OK,
                         (const uint8_t *)key.did, did_len);
}

static int handle_lock(idd_daemon_t *daemon)
{
    ids_lock(&daemon->store);
    return send_ok(daemon, IDD_OP_LOCK);
}

static int handle_unlock(idd_daemon_t *daemon)
{
    ids_unlock(&daemon->store);
    return send_ok(daemon, IDD_OP_UNLOCK);
}

static int handle_status(idd_daemon_t *daemon)
{
    uint8_t payload[1];
    payload[0] = ids_is_locked(&daemon->store) ? 1u : 0u;
    return send_response(daemon, IDD_OP_STATUS, IDD_STATUS_OK, payload, 1);
}

/* ========================================================================
 * PUBLIC API
 * ======================================================================== */

int idd_init(idd_daemon_t *daemon, const char *socket_path, const char *db_path)
{
    if (daemon == NULL || socket_path == NULL || db_path == NULL) {
        return IDD_ERR_NULL;
    }

    memset(daemon, 0, sizeof(*daemon));
    daemon->socket_path = socket_path;
    daemon->db_path = db_path;
    daemon->running = 0;

    return IDD_OK;
}

int idd_start(idd_daemon_t *daemon)
{
    int rc;

    if (daemon == NULL) { return IDD_ERR_NULL; }

    /* Open identity store */
    rc = ids_open(&daemon->store, daemon->db_path);
    if (rc != IDS_OK) { return IDD_ERR_STORE; }

    /* Generate sovereign key if not yet present */
    if (daemon->store.sovereign_key_id < 0) {
        ids_key_t sov;
        rc = ids_generate_sovereign(&daemon->store, &sov);
        if (rc != IDS_OK) {
            ids_close(&daemon->store);
            return IDD_ERR_STORE;
        }
        zako_sign_seckey_zero(sov.seckey);
    }

    /* Connect to bus */
    rc = zbu_client_init(&daemon->bus);
    if (rc != ZBU_OK) {
        ids_close(&daemon->store);
        return IDD_ERR_BUS;
    }

    rc = zbu_client_connect(&daemon->bus, daemon->socket_path);
    if (rc != ZBU_OK) {
        ids_close(&daemon->store);
        return IDD_ERR_BUS;
    }

    /* Subscribe to Identity category */
    rc = zbu_client_subscribe(&daemon->bus, IDD_CAT_IDENTITY);
    if (rc != ZBU_OK) {
        zbu_client_disconnect(&daemon->bus);
        ids_close(&daemon->store);
        return IDD_ERR_BUS;
    }

    daemon->running = 1;
    return IDD_OK;
}

int idd_poll(idd_daemon_t *daemon, int timeout_ms)
{
    zbu_frame_t frame;
    int processed = 0;
    int rc;

    if (daemon == NULL) { return IDD_ERR_NULL; }
    if (!daemon->running) { return IDD_ERR_SHUTDOWN; }

    (void)timeout_ms;

    rc = zbu_client_recv(&daemon->bus, &frame);
    if (rc == ZBU_ERR_AGAIN) { return 0; }
    if (rc == ZBU_ERR_CLOSED) {
        daemon->running = 0;
        return IDD_ERR_BUS;
    }
    if (rc != ZBU_OK) { return IDD_ERR_BUS; }

    daemon->requests_handled++;
    idd_process_frame(daemon, frame.data, frame.len);
    processed++;

    /* Drain additional frames */
    while (processed < 64) {
        rc = zbu_client_recv(&daemon->bus, &frame);
        if (rc != ZBU_OK) { break; }
        daemon->requests_handled++;
        idd_process_frame(daemon, frame.data, frame.len);
        processed++;
    }

    return processed;
}

int idd_run(idd_daemon_t *daemon)
{
    int rc;

    if (daemon == NULL) { return IDD_ERR_NULL; }

    while (daemon->running) {
        rc = idd_poll(daemon, IDD_POLL_TIMEOUT_MS);
        if (rc < 0 && rc != IDD_ERR_FRAME) {
            daemon->running = 0;
            return rc;
        }
    }

    return IDD_OK;
}

void idd_stop(idd_daemon_t *daemon)
{
    if (daemon == NULL) { return; }
    daemon->running = 0;
}

void idd_shutdown(idd_daemon_t *daemon)
{
    if (daemon == NULL) { return; }
    daemon->running = 0;
    zbu_client_disconnect(&daemon->bus);
    ids_close(&daemon->store);
}

int idd_process_frame(idd_daemon_t *daemon, const uint8_t *data, size_t len)
{
    uint8_t opcode;

    if (daemon == NULL || data == NULL || len == 0) { return IDD_ERR_NULL; }

    opcode = data[0];

    switch (opcode) {
    case IDD_OP_SIGN:
        return handle_sign(daemon, data + 1, len - 1);

    case IDD_OP_VERIFY:
        return handle_verify(daemon, data + 1, len - 1);

    case IDD_OP_KEYGEN:
        return handle_keygen(daemon, data + 1, len - 1);

    case IDD_OP_GET_SOVEREIGN:
        return handle_get_sovereign(daemon);

    case IDD_OP_LOCK:
        return handle_lock(daemon);

    case IDD_OP_UNLOCK:
        return handle_unlock(daemon);

    case IDD_OP_STATUS:
        return handle_status(daemon);

    default:
        return send_err(daemon, opcode, IDD_STATUS_ERR);
    }
}
