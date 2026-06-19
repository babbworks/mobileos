/*
 * exchange_engine.h — Bilateral Exchange Engine for ZAKO OS
 *
 * Manages the SEND/RECEIVE exchange cycle between two Sovereigns:
 *   - Outbound: creates SEND records, queues for transmission
 *   - Inbound: validates received frames, holds pending legs
 *   - Atomic posting: both legs posted together or neither
 *   - Conservation enforcement: SEND + RECEIVE must sum to zero
 *   - Signature verification: validates counterparty DID signature
 *
 * Exchange states:
 *   PENDING_OUTBOUND  — SEND created, awaiting counterparty ACK
 *   PENDING_INBOUND   — received frame, awaiting local ACK
 *   COMPLETED         — both legs posted atomically
 *   FAILED            — conservation check failed or timeout
 *   CANCELLED         — explicitly cancelled
 *
 * The engine is pure logic — it doesn't own transport or storage.
 * It calls out to the ledger (via function pointers) for posting,
 * and to the identity service for signature verification.
 *
 * Copyright (c) 2026 Babb Works. Licensed under MIT.
 */

#ifndef EXCHANGE_ENGINE_H
#define EXCHANGE_ENGINE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * CONSTANTS
 * ======================================================================== */

#define EXE_DID_MAX          64u   /* Max DID string length */
#define EXE_MAX_EXCHANGES    32u   /* Max concurrent pending exchanges */
#define EXE_FRAME_MAX        64u   /* Max frame bytes per leg */

/* Exchange states */
#define EXE_STATE_EMPTY              0u
#define EXE_STATE_PENDING_OUTBOUND   1u  /* We sent, awaiting ACK */
#define EXE_STATE_PENDING_INBOUND    2u  /* We received, awaiting our ACK */
#define EXE_STATE_COMPLETED          3u  /* Both legs posted */
#define EXE_STATE_FAILED             4u  /* Conservation failed */
#define EXE_STATE_CANCELLED          5u  /* Cancelled */

/* Error codes */
#define EXE_OK                0
#define EXE_ERR_NULL         (-1)
#define EXE_ERR_FULL         (-2)   /* Exchange table full */
#define EXE_ERR_NOT_FOUND    (-3)   /* Exchange ID not found */
#define EXE_ERR_STATE        (-4)   /* Invalid state transition */
#define EXE_ERR_CONSERVATION (-5)   /* Amounts don't balance */
#define EXE_ERR_VERIFY       (-6)   /* Signature verification failed */
#define EXE_ERR_POST         (-7)   /* Ledger posting failed */

/* ========================================================================
 * STRUCTURES
 * ======================================================================== */

/* A single exchange leg (one record in the exchange) */
typedef struct {
    uint8_t  frame[EXE_FRAME_MAX];  /* Raw BitLedger frame bytes */
    size_t   frame_len;
    char     did[EXE_DID_MAX];      /* DID of the party who created this leg */
    uint8_t  sig[64];               /* ed25519 signature over frame */
    uint32_t value_n;               /* Extracted value from record */
    uint8_t  direction;             /* 0=in (credit to us), 1=out (debit from us) */
    int64_t  timestamp;             /* When this leg was created */
} exe_leg_t;

/* A bilateral exchange (two legs: outbound + inbound) */
typedef struct {
    uint32_t  id;                   /* Unique exchange ID */
    uint8_t   state;                /* EXE_STATE_* */

    exe_leg_t local_leg;            /* Our record (SEND or RECEIVE) */
    uint8_t   local_valid;          /* 1 if local_leg is populated */

    exe_leg_t remote_leg;           /* Counterparty's record */
    uint8_t   remote_valid;         /* 1 if remote_leg is populated */

    char      counterparty_did[EXE_DID_MAX]; /* Who we're exchanging with */
    int64_t   created;              /* When exchange was initiated */
} exe_exchange_t;

/* ========================================================================
 * CALLBACKS (for ledger + identity integration)
 * ======================================================================== */

typedef struct {
    /*
     * Post a pair of records atomically to the ledger.
     * Returns 0 on success. Both frames are posted or neither.
     *
     * Merge-like posting: frame_b is posted as a merge record with
     * frame_a's chain_hash as parent2 (dual-parent chain link).
     * This mirrors git's merge commit with two parent hashes.
     */
    int (*post_atomic)(const uint8_t *frame_a, size_t len_a,
                       const uint8_t *frame_b, size_t len_b,
                       uint32_t sender_id, void *ctx);

    /*
     * Verify a signature against a DID.
     * Returns 0 if valid.
     */
    int (*verify_sig)(const char *did,
                      const uint8_t *message, size_t msg_len,
                      const uint8_t *sig, void *ctx);

    void *ctx;
} exe_callbacks_t;

/* Engine state */
typedef struct {
    exe_exchange_t  exchanges[EXE_MAX_EXCHANGES];
    uint32_t        next_id;
    exe_callbacks_t callbacks;

    /* Stats */
    uint64_t        completed_count;
    uint64_t        failed_count;
    uint64_t        cancelled_count;
} exe_engine_t;

/* ========================================================================
 * PUBLIC API
 * ======================================================================== */

/*
 * exe_init — Initialize the exchange engine.
 */
int exe_init(exe_engine_t *engine, const exe_callbacks_t *callbacks);

/*
 * exe_send — Create an outbound exchange (we are sending).
 *
 * Creates a SEND leg signed by our key. Moves to PENDING_OUTBOUND.
 * Caller should transmit the frame via sharedb after this returns.
 *
 * @param engine          Engine instance
 * @param frame           Our SEND record frame bytes
 * @param frame_len       Frame length
 * @param our_did         Our DID (signer)
 * @param our_sig         Our signature over the frame
 * @param counterparty    Counterparty DID
 * @param value_n         Value being sent
 * @param out_id          Output: exchange ID for tracking
 * @return EXE_OK on success
 */
int exe_send(exe_engine_t *engine,
             const uint8_t *frame, size_t frame_len,
             const char *our_did, const uint8_t *our_sig,
             const char *counterparty,
             uint32_t value_n, uint32_t *out_id);

/*
 * exe_receive — Process an inbound frame from a counterparty.
 *
 * Validates signature, creates a PENDING_INBOUND exchange (or matches
 * to an existing PENDING_OUTBOUND if this is the ACK to our SEND).
 *
 * @param engine          Engine instance
 * @param frame           Received frame bytes
 * @param frame_len       Frame length
 * @param sender_did      DID of the sender (from frame or transport)
 * @param sig             Sender's signature over the frame
 * @param value_n         Value in the received record
 * @param direction       Direction from OUR perspective (0=in, 1=out)
 * @param out_id          Output: exchange ID
 * @return EXE_OK on success, EXE_ERR_VERIFY if sig fails
 */
int exe_receive(exe_engine_t *engine,
                const uint8_t *frame, size_t frame_len,
                const char *sender_did, const uint8_t *sig,
                uint32_t value_n, uint8_t direction,
                uint32_t *out_id);

/*
 * exe_acknowledge — Acknowledge a PENDING_INBOUND exchange.
 *
 * Creates our ACK leg, runs conservation check, and if balanced,
 * posts both legs atomically to the ledger.
 *
 * @param engine          Engine instance
 * @param exchange_id     ID of the exchange to acknowledge
 * @param ack_frame       Our ACK/RECEIVE frame bytes
 * @param ack_len         Frame length
 * @param our_did         Our DID
 * @param our_sig         Our signature over ack_frame
 * @param our_value       Value in our leg (for conservation check)
 * @param our_direction   Our direction (should be opposite of remote)
 * @return EXE_OK if posted, EXE_ERR_CONSERVATION if imbalanced
 */
int exe_acknowledge(exe_engine_t *engine, uint32_t exchange_id,
                    const uint8_t *ack_frame, size_t ack_len,
                    const char *our_did, const uint8_t *our_sig,
                    uint32_t our_value, uint8_t our_direction);

/*
 * exe_complete_outbound — Complete a PENDING_OUTBOUND exchange.
 *
 * Called when we receive the counterparty's ACK to our SEND.
 * Verifies the counterparty's signature, runs conservation, posts atomically.
 *
 * @param engine       Engine instance
 * @param exchange_id  ID of the outbound exchange
 * @param ack_frame    Counterparty's ACK frame
 * @param ack_len      Frame length
 * @param ack_did      Counterparty DID (for sig verification)
 * @param ack_sig      Counterparty's signature
 * @param ack_value    Value in the ACK record
 * @param ack_dir      Direction of ACK from our perspective
 * @return EXE_OK if posted, EXE_ERR_CONSERVATION if imbalanced
 */
int exe_complete_outbound(exe_engine_t *engine, uint32_t exchange_id,
                          const uint8_t *ack_frame, size_t ack_len,
                          const char *ack_did, const uint8_t *ack_sig,
                          uint32_t ack_value, uint8_t ack_dir);

/*
 * exe_cancel — Cancel a pending exchange.
 */
int exe_cancel(exe_engine_t *engine, uint32_t exchange_id);

/*
 * exe_get_state — Query exchange state.
 */
int exe_get_state(exe_engine_t *engine, uint32_t exchange_id, uint8_t *out_state);

/*
 * exe_get_pending_count — Number of exchanges in pending state.
 */
size_t exe_get_pending_count(exe_engine_t *engine);

#ifdef __cplusplus
}
#endif

#endif /* EXCHANGE_ENGINE_H */
