/*
 * ledgerd_daemon.c — telux-ledgerd Daemon Shell Implementation
 *
 * Event loop connecting the system bus to the ledger storage layer.
 * Parses incoming BitPads frames as BitLedger records, validates,
 * stores, and sends ACK/NAK responses.
 *
 * Frame dispatch logic:
 *   - 1 byte frame  → Control record (session/batch open/close, ACK/NAK)
 *   - 5 byte frame  → Layer 3 transaction record
 *   - 6 byte frame  → Layer 2 batch header
 *   - 11 byte frame → Layer 2 header + Layer 3 record (combined)
 *
 * Copyright (c) 2026 Babb Works. Licensed under MIT.
 */

#include "ledgerd_daemon.h"

#include <string.h>

/* ========================================================================
 * INTERNAL HELPERS
 * ======================================================================== */

/*
 * Send an ACK control frame back through the bus.
 */
static int send_ack(ldd_daemon_t *daemon)
{
    uint8_t ctrl = zbl_control_encode(ZBL_CTRL_ACK, 0);
    return zbu_client_send(&daemon->bus, &ctrl, 1);
}

/*
 * Send a NAK control frame with a reason code.
 * Reason codes (5-bit payload):
 *   0x01 = cross-layer validation failure
 *   0x02 = rounding validity failure
 *   0x03 = store error (chain/db)
 *   0x04 = conservation failure (batch imbalanced)
 *   0x05 = frame size/parse error
 *   0x06 = no batch open (record outside batch)
 *   0x07 = batch record limit exceeded
 */
static int send_nak(ldd_daemon_t *daemon, uint8_t reason)
{
    uint8_t ctrl = zbl_control_encode(ZBL_CTRL_NAK, reason & 0x1Fu);
    return zbu_client_send(&daemon->bus, &ctrl, 1);
}

/*
 * Process a control record (1 byte).
 */
static int process_control(ldd_daemon_t *daemon, uint8_t byte)
{
    zbl_control_t ctrl;
    int rc;

    rc = zbl_control_decode(byte, &ctrl);
    if (rc != ZBL_OK) { return LDD_ERR_FRAME; }

    switch (ctrl.type) {
    case ZBL_CTRL_BATCH_OPEN: {
        /* Open a new batch for conservation tracking */
        int64_t batch_id;
        rc = lds_batch_open(&daemon->store, &batch_id);
        if (rc != LDS_OK) {
            send_nak(daemon, 0x03);
            return LDD_ERR_STORE;
        }
        daemon->batch_open = 1;
        daemon->batch_record_count = 0;
        send_ack(daemon);
        return LDD_OK;
    }

    case ZBL_CTRL_BATCH_CLOSE: {
        /* Close batch: run conservation check */
        int64_t balance;

        if (!daemon->batch_open) {
            send_nak(daemon, 0x06);
            return LDD_ERR_FRAME;
        }

        /* Conservation check via libzako-bitledger on tracked records */
        if (daemon->batch_record_count > 0) {
            int64_t check_balance;
            rc = zbl_conservation_check(daemon->batch_records,
                                        daemon->batch_record_count,
                                        &check_balance);
            if (rc != ZBL_OK) {
                /* Imbalanced — reject the batch close */
                send_nak(daemon, 0x04);
                daemon->batches_closed++;
                /* Leave batch open for correction or forced close */
                return LDD_ERR_FRAME;
            }
        }

        /* Close in store (also checks via running balance) */
        rc = lds_batch_close(&daemon->store, &balance);
        daemon->batch_open = 0;
        daemon->batch_record_count = 0;
        daemon->batches_closed++;

        if (rc == LDS_OK) {
            daemon->batches_conserved++;
            send_ack(daemon);
            return LDD_OK;
        } else {
            /* Store-level conservation failure */
            send_nak(daemon, 0x04);
            return LDD_ERR_FRAME;
        }
    }

    case ZBL_CTRL_SESSION_OPEN:
        /* Reset session state */
        daemon->header_valid = 0;
        daemon->batch_open = 0;
        daemon->batch_record_count = 0;
        send_ack(daemon);
        return LDD_OK;

    case ZBL_CTRL_SESSION_CLOSE:
        /* If batch still open, force-close with whatever balance */
        if (daemon->batch_open) {
            int64_t bal;
            lds_batch_close(&daemon->store, &bal);
            daemon->batch_open = 0;
            daemon->batch_record_count = 0;
        }
        daemon->header_valid = 0;
        send_ack(daemon);
        return LDD_OK;

    case ZBL_CTRL_ACK:
    case ZBL_CTRL_NAK:
    case ZBL_CTRL_STATUS:
    case ZBL_CTRL_EXTENSION:
        /* Informational — acknowledge receipt */
        return LDD_OK;

    default:
        return LDD_OK;
    }
}

/*
 * Process a Layer 2 batch header (6 bytes).
 */
static int process_layer2(ldd_daemon_t *daemon, const uint8_t *data)
{
    int rc = zbl_layer2_decode(data, &daemon->current_header);
    if (rc != ZBL_OK) {
        send_nak(daemon, 0x05);
        return LDD_ERR_FRAME;
    }

    daemon->header_valid = 1;
    send_ack(daemon);
    return LDD_OK;
}

/*
 * Process a Layer 3 transaction record (5 bytes).
 * Validates, stores, and tracks for conservation.
 */
static int process_layer3(ldd_daemon_t *daemon, const uint8_t *data,
                          uint32_t sender_id)
{
    zbl_record_t rec;
    uint8_t split_s;
    int rc;
    int64_t seq;

    /* Use split from current header, or default 8 */
    split_s = daemon->header_valid ? daemon->current_header.optimal_split : 8u;

    /* Decode the record */
    rc = zbl_record_decode(data, split_s, &rec);
    if (rc != ZBL_OK) {
        send_nak(daemon, 0x05);
        daemon->records_rejected++;
        return LDD_ERR_FRAME;
    }

    /* Cross-layer validation */
    if (!rec.crosslayer_valid) {
        send_nak(daemon, 0x01);
        daemon->records_rejected++;
        return LDD_ERR_FRAME;
    }

    /* Rounding validity */
    if (!rec.rounding_valid) {
        send_nak(daemon, 0x02);
        daemon->records_rejected++;
        return LDD_ERR_FRAME;
    }

    /* Must have a batch open to accept records */
    if (!daemon->batch_open) {
        send_nak(daemon, 0x06);
        daemon->records_rejected++;
        return LDD_ERR_FRAME;
    }

    /* Track for conservation check (if space) */
    if (daemon->batch_record_count >= LDD_MAX_BATCH_RECORDS) {
        send_nak(daemon, 0x07);
        daemon->records_rejected++;
        return LDD_ERR_FRAME;
    }

    /* Submit to store */
    rc = lds_append(&daemon->store, data, ZBL_LAYER3_SIZE,
                    sender_id, rec.direction, rec.value_n, &seq);
    if (rc != LDS_OK) {
        send_nak(daemon, 0x03);
        daemon->records_rejected++;
        return LDD_ERR_STORE;
    }

    /* Track record for batch conservation */
    daemon->batch_records[daemon->batch_record_count] = rec;
    daemon->batch_record_count++;

    daemon->records_accepted++;
    send_ack(daemon);
    return LDD_OK;
}

/* ========================================================================
 * PUBLIC API
 * ======================================================================== */

int ldd_init(ldd_daemon_t *daemon, const char *socket_path, const char *db_path)
{
    if (daemon == NULL || socket_path == NULL || db_path == NULL) {
        return LDD_ERR_NULL;
    }

    memset(daemon, 0, sizeof(*daemon));
    daemon->socket_path = socket_path;
    daemon->db_path = db_path;
    daemon->running = 0;
    daemon->header_valid = 0;
    daemon->batch_open = 0;
    daemon->batch_record_count = 0;

    return LDD_OK;
}

int ldd_start(ldd_daemon_t *daemon)
{
    int rc;

    if (daemon == NULL) { return LDD_ERR_NULL; }

    /* Open database */
    rc = lds_open(&daemon->store, daemon->db_path);
    if (rc != LDS_OK) { return LDD_ERR_STORE; }

    /* Connect to bus */
    rc = zbu_client_init(&daemon->bus);
    if (rc != ZBU_OK) {
        lds_close(&daemon->store);
        return LDD_ERR_BUS;
    }

    rc = zbu_client_connect(&daemon->bus, daemon->socket_path);
    if (rc != ZBU_OK) {
        lds_close(&daemon->store);
        return LDD_ERR_BUS;
    }

    /* Subscribe to relevant categories */
    rc = zbu_client_subscribe(&daemon->bus, LDD_CAT_FINANCIAL);
    if (rc != ZBU_OK) {
        zbu_client_disconnect(&daemon->bus);
        lds_close(&daemon->store);
        return LDD_ERR_BUS;
    }

    rc = zbu_client_subscribe(&daemon->bus, LDD_CAT_DATA_TRANSFER);
    if (rc != ZBU_OK) {
        zbu_client_disconnect(&daemon->bus);
        lds_close(&daemon->store);
        return LDD_ERR_BUS;
    }

    daemon->running = 1;
    return LDD_OK;
}

int ldd_poll(ldd_daemon_t *daemon, int timeout_ms)
{
    zbu_frame_t frame;
    int processed = 0;
    int rc;

    if (daemon == NULL) { return LDD_ERR_NULL; }
    if (!daemon->running) { return LDD_ERR_SHUTDOWN; }

    /* Attempt to receive a frame (non-blocking read inside) */
    rc = zbu_client_recv(&daemon->bus, &frame);

    if (rc == ZBU_ERR_AGAIN) {
        /* No frame ready — this is normal */
        (void)timeout_ms; /* timeout handled by caller's sleep/poll strategy */
        return 0;
    }

    if (rc == ZBU_ERR_CLOSED) {
        /* Bus disconnected */
        daemon->running = 0;
        return LDD_ERR_BUS;
    }

    if (rc != ZBU_OK) {
        return LDD_ERR_BUS;
    }

    /* Frame received — dispatch by size */
    daemon->frames_received++;
    ldd_process_frame(daemon, frame.data, frame.len);
    processed++;

    /* Drain any additional queued frames */
    while (processed < 64) {
        rc = zbu_client_recv(&daemon->bus, &frame);
        if (rc != ZBU_OK) { break; }

        daemon->frames_received++;
        ldd_process_frame(daemon, frame.data, frame.len);
        processed++;
    }

    return processed;
}

int ldd_run(ldd_daemon_t *daemon)
{
    int rc;

    if (daemon == NULL) { return LDD_ERR_NULL; }

    while (daemon->running) {
        rc = ldd_poll(daemon, LDD_POLL_TIMEOUT_MS);
        if (rc < 0 && rc != LDD_ERR_FRAME) {
            /* Fatal error — stop loop */
            daemon->running = 0;
            return rc;
        }
        /* rc == 0 means no frames this cycle — loop again */
        /* rc > 0 means frames processed — loop again */
        /* rc == LDD_ERR_FRAME means validation failure but loop continues */
    }

    return LDD_OK;
}

void ldd_stop(ldd_daemon_t *daemon)
{
    if (daemon == NULL) { return; }
    daemon->running = 0;
}

void ldd_shutdown(ldd_daemon_t *daemon)
{
    if (daemon == NULL) { return; }

    daemon->running = 0;

    /* Close batch if still open */
    if (daemon->batch_open) {
        int64_t bal;
        lds_batch_close(&daemon->store, &bal);
        daemon->batch_open = 0;
    }

    /* Disconnect from bus */
    zbu_client_disconnect(&daemon->bus);

    /* Close database */
    lds_close(&daemon->store);
}

int ldd_process_frame(ldd_daemon_t *daemon, const uint8_t *data, size_t len)
{
    if (daemon == NULL || data == NULL || len == 0) {
        return LDD_ERR_NULL;
    }

    switch (len) {
    case ZBL_CONTROL_SIZE:
        /* 1 byte: control record */
        return process_control(daemon, data[0]);

    case ZBL_LAYER3_SIZE:
        /* 5 bytes: Layer 3 transaction record */
        return process_layer3(daemon, data, 0);

    case ZBL_LAYER2_SIZE:
        /* 6 bytes: Layer 2 batch header */
        return process_layer2(daemon, data);

    case (ZBL_LAYER2_SIZE + ZBL_LAYER3_SIZE): {
        /* 11 bytes: combined Layer 2 header + Layer 3 record */
        int rc = process_layer2(daemon, data);
        if (rc != LDD_OK) { return rc; }
        return process_layer3(daemon, data + ZBL_LAYER2_SIZE, 0);
    }

    default:
        /* Unknown frame size — NAK */
        send_nak(daemon, 0x05);
        return LDD_ERR_FRAME;
    }
}
