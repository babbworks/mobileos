/*
 * identd_daemon.h — telux-identd Daemon Shell
 *
 * Bus-connected event loop handling identity requests from other daemons:
 *   - SIGN requests (sign a message, return signature)
 *   - VERIFY requests (verify a signature against a DID)
 *   - KEY_GEN requests (generate a new key, return DID)
 *   - GRANT/REVOKE capability management
 *   - LOCK/UNLOCK identity state changes
 *
 * Request/Response protocol (via bus frames):
 *   Request:  [1-byte opcode][payload...]
 *   Response: [1-byte opcode | 0x80][status][payload...]
 *
 * Subscribes to bus category 3 (Identity/Auth).
 *
 * Copyright (c) 2026 Babb Works. Licensed under MIT.
 */

#ifndef IDENTD_DAEMON_H
#define IDENTD_DAEMON_H

#include <stdint.h>

#include "../libzako-bus/zako_bus.h"
#include "identd_store.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * PROTOCOL OPCODES
 * ======================================================================== */

#define IDD_OP_SIGN          0x01u  /* Sign a message */
#define IDD_OP_VERIFY        0x02u  /* Verify a signature */
#define IDD_OP_KEYGEN        0x03u  /* Generate new key */
#define IDD_OP_GET_SOVEREIGN 0x04u  /* Get sovereign DID */
#define IDD_OP_GRANT         0x05u  /* Grant capability */
#define IDD_OP_REVOKE        0x06u  /* Revoke capability */
#define IDD_OP_CHECK_CAP     0x07u  /* Check capability */
#define IDD_OP_LOCK          0x08u  /* Lock identity */
#define IDD_OP_UNLOCK        0x09u  /* Unlock identity */
#define IDD_OP_STATUS        0x0Au  /* Status query */

/* Response = opcode | 0x80 */
#define IDD_RESP_BIT         0x80u

/* Status codes in response payload */
#define IDD_STATUS_OK        0x00u
#define IDD_STATUS_ERR       0x01u
#define IDD_STATUS_LOCKED    0x02u
#define IDD_STATUS_NOT_FOUND 0x03u
#define IDD_STATUS_REVOKED   0x04u
#define IDD_STATUS_DEPTH     0x05u

/* Bus category for identity/auth */
#define IDD_CAT_IDENTITY     3u

/* ========================================================================
 * CONFIGURATION
 * ======================================================================== */

#ifndef IDD_POLL_TIMEOUT_MS
#define IDD_POLL_TIMEOUT_MS  100
#endif

/* ========================================================================
 * ERROR CODES
 * ======================================================================== */

#define IDD_OK              0
#define IDD_ERR_NULL       (-1)
#define IDD_ERR_BUS        (-2)
#define IDD_ERR_STORE      (-3)
#define IDD_ERR_FRAME      (-4)
#define IDD_ERR_SHUTDOWN   (-5)

/* ========================================================================
 * DAEMON STATE
 * ======================================================================== */

typedef struct {
    zbu_conn_t    bus;
    const char   *socket_path;
    ids_store_t   store;
    const char   *db_path;
    uint8_t       running;

    /* Stats */
    uint64_t      requests_handled;
    uint64_t      sign_count;
    uint64_t      verify_count;
    uint64_t      keygen_count;
} idd_daemon_t;

/* ========================================================================
 * PUBLIC API
 * ======================================================================== */

int idd_init(idd_daemon_t *daemon, const char *socket_path, const char *db_path);
int idd_start(idd_daemon_t *daemon);
int idd_poll(idd_daemon_t *daemon, int timeout_ms);
int idd_run(idd_daemon_t *daemon);
void idd_stop(idd_daemon_t *daemon);
void idd_shutdown(idd_daemon_t *daemon);

/*
 * idd_process_frame — Process a single request frame. Exposed for testing.
 *
 * @param daemon   Running daemon
 * @param data     Frame payload (opcode + args)
 * @param len      Frame length
 * @return IDD_OK on success
 */
int idd_process_frame(idd_daemon_t *daemon, const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* IDENTD_DAEMON_H */
