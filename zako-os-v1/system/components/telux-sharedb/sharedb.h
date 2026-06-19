/*
 * sharedb.h — Telux Share Daemon (Channel Abstraction) for ZAKO OS
 *
 * Manages outbound/inbound frame transport across multiple carriers:
 *   - SMS (primary for Zambia, uses pads-v1 URL encoding)
 *   - IP (TCP/HTTPS to relay, raw BitPads frames)
 *   - BLE (short-range direct, raw frames)
 *   - QR (display-only, always available, zero radio cost)
 *
 * Architecture:
 *   - Durable outbound queue (survives restart, FIFO, bounded)
 *   - Carrier abstraction (function pointer table per channel)
 *   - Transport selection (mode-aware, counterparty-aware)
 *   - Bus integration (subscribes to Exchange category)
 *   - Power mode integration (adapts selection to Outstack mode)
 *
 * Queue entry lifecycle:
 *   QUEUED → SENDING → SENT → ACKED (or FAILED after retries)
 *
 * Copyright (c) 2026 Babb Works. Licensed under MIT.
 */

#ifndef SHAREDB_H
#define SHAREDB_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * CONSTANTS
 * ======================================================================== */

#define SDB_MAX_QUEUE        1024u  /* Max pending outbound entries */
#define SDB_FRAME_MAX        256u   /* Max frame bytes per entry */
#define SDB_DID_MAX          64u    /* Max DID string */
#define SDB_ENDPOINT_MAX     128u   /* Max endpoint string (phone#, URL) */

/* Carrier types */
#define SDB_CARRIER_NONE     0u
#define SDB_CARRIER_SMS      1u
#define SDB_CARRIER_IP       2u
#define SDB_CARRIER_BLE      3u
#define SDB_CARRIER_QR       4u

/* Queue entry states */
#define SDB_ENTRY_EMPTY      0u
#define SDB_ENTRY_QUEUED     1u
#define SDB_ENTRY_SENDING    2u
#define SDB_ENTRY_SENT       3u
#define SDB_ENTRY_ACKED      4u
#define SDB_ENTRY_FAILED     5u

/* Power modes (matching outstack-powerd) */
#define SDB_MODE_FULL        0u
#define SDB_MODE_STD         1u
#define SDB_MODE_CONS        2u
#define SDB_MODE_CRIT        3u
#define SDB_MODE_EMRG        4u

/* Error codes */
#define SDB_OK               0
#define SDB_ERR_NULL        (-1)
#define SDB_ERR_FULL        (-2)   /* Queue full */
#define SDB_ERR_NOT_FOUND   (-3)
#define SDB_ERR_CARRIER     (-4)   /* No carrier available */
#define SDB_ERR_ENCODE      (-5)   /* Encoding failed */
#define SDB_ERR_SEND        (-6)   /* Carrier send failed */

/* ========================================================================
 * STRUCTURES
 * ======================================================================== */

/* Counterparty endpoint info */
typedef struct {
    char     did[SDB_DID_MAX];
    uint8_t  has_sms;               /* Can receive SMS */
    char     sms_endpoint[SDB_ENDPOINT_MAX]; /* Phone number */
    uint8_t  has_ip;                /* Can receive IP */
    char     ip_endpoint[SDB_ENDPOINT_MAX];  /* Relay URL */
    uint8_t  has_ble;               /* Can receive BLE */
} sdb_peer_t;

/* A queued outbound entry */
typedef struct {
    uint32_t id;
    uint8_t  state;                 /* SDB_ENTRY_* */
    uint8_t  frame[SDB_FRAME_MAX]; /* Raw frame bytes */
    size_t   frame_len;
    sdb_peer_t peer;                /* Destination */
    uint8_t  carrier_used;          /* Which carrier was selected */
    uint8_t  retries;               /* Send attempts */
    int64_t  queued_at;             /* Timestamp */
    int64_t  sent_at;               /* 0 if not yet sent */
} sdb_entry_t;

/* ========================================================================
 * CARRIER ABSTRACTION
 *
 * Each carrier implements send(). On real hardware, this talks to
 * RIL/sockets/BLE. In tests, it's a loopback mock.
 * ======================================================================== */

typedef struct {
    /*
     * Send a frame via this carrier.
     * For SMS: frame is pads-v1 URL encoded before calling this.
     * For IP/BLE: frame is raw BitPads bytes.
     *
     * @param payload     Encoded payload (URL string for SMS, raw for IP/BLE)
     * @param payload_len Payload length
     * @param endpoint    Destination (phone# for SMS, URL for IP)
     * @param ctx         Carrier-specific context
     * @return 0 on success
     */
    int (*send)(const uint8_t *payload, size_t payload_len,
                const char *endpoint, void *ctx);

    /* Is this carrier currently available? */
    int (*available)(void *ctx);

    void *ctx;
    uint8_t carrier_type;  /* SDB_CARRIER_* */
} sdb_carrier_t;

/* ========================================================================
 * SHAREDB STATE
 * ======================================================================== */

#ifndef SDB_MAX_CARRIERS
#define SDB_MAX_CARRIERS 4u
#endif

typedef struct {
    /* Queue (circular buffer semantics but using linear scan for simplicity) */
    sdb_entry_t  queue[SDB_MAX_QUEUE];
    uint32_t     next_id;

    /* Carriers */
    sdb_carrier_t carriers[SDB_MAX_CARRIERS];
    uint8_t       carrier_count;

    /* Current power mode (affects transport selection) */
    uint8_t       power_mode;

    /* Stats */
    uint64_t      enqueued;
    uint64_t      sent;
    uint64_t      failed;
    uint64_t      received;
} sdb_state_t;

/* ========================================================================
 * PUBLIC API
 * ======================================================================== */

/*
 * sdb_init — Initialize sharedb state.
 */
int sdb_init(sdb_state_t *state);

/*
 * sdb_add_carrier — Register a carrier backend.
 */
int sdb_add_carrier(sdb_state_t *state, const sdb_carrier_t *carrier);

/*
 * sdb_set_power_mode — Update current Outstack power mode.
 * Affects transport selection on next send.
 */
int sdb_set_power_mode(sdb_state_t *state, uint8_t mode);

/*
 * sdb_enqueue — Queue a frame for outbound delivery.
 *
 * @param state      Sharedb state
 * @param frame      Raw BitPads/BitLedger frame bytes
 * @param frame_len  Frame length
 * @param peer       Counterparty endpoint info
 * @param out_id     Output: queue entry ID
 * @return SDB_OK or SDB_ERR_FULL
 */
int sdb_enqueue(sdb_state_t *state,
                const uint8_t *frame, size_t frame_len,
                const sdb_peer_t *peer, uint32_t *out_id);

/*
 * sdb_process_queue — Attempt to send all QUEUED entries.
 *
 * Selects carrier per entry based on peer capabilities + power mode.
 * Moves entries from QUEUED → SENDING → SENT (or FAILED).
 *
 * @param state  Sharedb state
 * @return Number of entries successfully sent
 */
int sdb_process_queue(sdb_state_t *state);

/*
 * sdb_select_carrier — Determine best carrier for a peer in current mode.
 *
 * @param state  Sharedb state
 * @param peer   Counterparty info
 * @return Carrier type (SDB_CARRIER_*), or SDB_CARRIER_NONE if unavailable
 */
uint8_t sdb_select_carrier(sdb_state_t *state, const sdb_peer_t *peer);

/*
 * sdb_get_queue_depth — Number of entries in QUEUED state.
 */
size_t sdb_get_queue_depth(sdb_state_t *state);

/*
 * sdb_get_entry_state — Query a queue entry state by ID.
 */
int sdb_get_entry_state(sdb_state_t *state, uint32_t id, uint8_t *out_state);

/*
 * sdb_mark_acked — Mark an entry as acknowledged (exchange completed).
 */
int sdb_mark_acked(sdb_state_t *state, uint32_t id);

#ifdef __cplusplus
}
#endif

#endif /* SHAREDB_H */
