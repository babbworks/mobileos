/*
 * ledgerd_daemon.h — telux-ledgerd Daemon Shell
 *
 * Thin event loop connecting the system bus to the ledger storage layer.
 * Receives BitPads frames from libzako-bus, parses them as BitLedger
 * Layer 3 records, validates, and submits to ledgerd_store.
 *
 * Architecture:
 *   - Single-threaded poll loop (driven by bus client recv)
 *   - Subscribes to Financial (category 1) and Data Transfer (category 2)
 *   - Parses incoming frames as: Layer 2 header, Layer 3 records, control
 *   - Validates: cross-layer check, rounding validity
 *   - Submits valid records to ledgerd_store (chain hash + SQLite)
 *   - Sends ACK/NAK control frames back through the bus
 *   - On BATCH_CLOSE control: runs conservation check, NAK if imbalanced
 *
 * No dynamic allocation. No threads.
 *
 * Copyright (c) 2026 Babb Works. Licensed under MIT.
 */

#ifndef LEDGERD_DAEMON_H
#define LEDGERD_DAEMON_H

#include <stdint.h>

#include "../libzako-bus/zako_bus.h"
#include "../libzako-bitledger/zako_bitledger.h"
#include "ledgerd_store.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * CONFIGURATION
 * ======================================================================== */

#ifndef LDD_POLL_TIMEOUT_MS
#define LDD_POLL_TIMEOUT_MS  100   /* Bus poll interval (ms) */
#endif

#ifndef LDD_MAX_BATCH_RECORDS
#define LDD_MAX_BATCH_RECORDS 256u /* Max records tracked per batch for conservation */
#endif

/* Wave Role B category codes for subscription */
#define LDD_CAT_FINANCIAL      1u  /* Financial transaction frames */
#define LDD_CAT_DATA_TRANSFER  2u  /* Data transfer / sync frames */

/* ========================================================================
 * ERROR CODES
 * ======================================================================== */

#define LDD_OK               0
#define LDD_ERR_NULL        (-1)
#define LDD_ERR_BUS         (-2)   /* Bus connection/communication failure */
#define LDD_ERR_STORE       (-3)   /* Storage layer failure */
#define LDD_ERR_FRAME       (-4)   /* Frame parse/validation failure */
#define LDD_ERR_CONFIG      (-5)   /* Invalid configuration */
#define LDD_ERR_SHUTDOWN    (-6)   /* Clean shutdown requested */

/* ========================================================================
 * DAEMON STATE
 * ======================================================================== */

typedef struct {
    /* Bus connection */
    zbu_conn_t       bus;
    const char      *socket_path;

    /* Storage */
    lds_store_t      store;
    const char      *db_path;
    int64_t          chain_id;        /* Active chain (Island) for this daemon */

    /* Session state */
    zbl_layer2_t     current_header;   /* Active batch header (from last Layer 2) */
    uint8_t          header_valid;     /* 1 if current_header has been set */
    uint8_t          batch_open;       /* 1 if a batch is currently open */

    /* Batch record tracking (for conservation on close) */
    zbl_record_t     batch_records[LDD_MAX_BATCH_RECORDS];
    size_t           batch_record_count;

    /* Runtime */
    uint8_t          running;          /* 1 while event loop active */

    /* Stats */
    uint64_t         frames_received;
    uint64_t         records_accepted;
    uint64_t         records_rejected;
    uint64_t         batches_closed;
    uint64_t         batches_conserved;
} ldd_daemon_t;

/* ========================================================================
 * PUBLIC API
 * ======================================================================== */

/*
 * ldd_init — Initialize daemon state.
 *
 * Does NOT connect to bus or open database — call ldd_start() for that.
 *
 * @param daemon       Daemon instance to initialize
 * @param socket_path  Bus socket path
 * @param db_path      SQLite database path (or ":memory:" for testing)
 * @return LDD_OK on success
 */
int ldd_init(ldd_daemon_t *daemon, const char *socket_path, const char *db_path);

/*
 * ldd_start — Connect to bus, open database, subscribe to categories.
 *
 * @param daemon  Initialized daemon
 * @return LDD_OK on success, LDD_ERR_BUS or LDD_ERR_STORE on failure
 */
int ldd_start(ldd_daemon_t *daemon);

/*
 * ldd_poll — Run one iteration of the event loop.
 *
 * Receives frames from bus, processes them, sends ACK/NAK.
 * Non-blocking with configurable timeout.
 *
 * @param daemon      Running daemon
 * @param timeout_ms  Poll timeout (0=immediate, -1=infinite)
 * @return Number of frames processed (>=0), or negative error
 */
int ldd_poll(ldd_daemon_t *daemon, int timeout_ms);

/*
 * ldd_run — Run the event loop until shutdown.
 *
 * Blocks until ldd_stop() is called (e.g., from a signal handler).
 *
 * @param daemon  Started daemon
 * @return LDD_OK on clean shutdown, or error code
 */
int ldd_run(ldd_daemon_t *daemon);

/*
 * ldd_stop — Request clean shutdown of the event loop.
 *
 * Safe to call from a signal handler (only sets a flag).
 */
void ldd_stop(ldd_daemon_t *daemon);

/*
 * ldd_shutdown — Disconnect from bus, close database, release resources.
 */
void ldd_shutdown(ldd_daemon_t *daemon);

/*
 * ldd_process_frame — Process a single received frame.
 *
 * Exposed for testing — normally called internally by ldd_poll().
 *
 * @param daemon  Running daemon
 * @param data    Frame payload bytes
 * @param len     Frame payload length
 * @return LDD_OK if processed successfully (ACK sent),
 *         LDD_ERR_FRAME if validation failed (NAK sent)
 */
int ldd_process_frame(ldd_daemon_t *daemon, const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* LEDGERD_DAEMON_H */
