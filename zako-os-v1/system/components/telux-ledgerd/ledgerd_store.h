/*
 * ledgerd_store.h — SQLite Storage Layer for telux-ledgerd
 *
 * Manages the append-only ledger database:
 *   - Multi-chain support (Islands as independent chains)
 *   - Record insertion with chain hash computation
 *   - Optional per-record ed25519 signatures (signed commits)
 *   - Merge-like bilateral exchange records (dual-parent chain hash)
 *   - Record retrieval by hash, sequence, or range
 *   - Cursor-based chain integrity verification
 *   - Batch tracking for conservation enforcement
 *   - Pack-like compaction of historical records
 *
 * The database is append-only: records are never modified or deleted.
 * Chain hashes form tamper-evident sequences per chain (Island).
 *
 * Git-informed design: content-addressed records, parent-linked chains,
 * branches-as-Islands, signed commits, merge commits for bilateral
 * exchanges, pack-file-like compaction. All implemented natively in
 * C + SQLite without libgit2 dependency.
 *
 * Schema:
 *   chains(id, name, genesis_hash, tip_hash, tip_seq, created)
 *
 *   records(seq, chain_id, frame, frame_hash, chain_hash,
 *           parent2_chain_hash, sender_id, batch_id, direction,
 *           value_n, timestamp, signature, packed)
 *
 *   batches(id, chain_id, opened, closed, record_count, balance,
 *           conserved)
 *
 *   packs(id, chain_id, start_seq, end_seq, record_count,
 *         pack_blob, index_blob, created)
 *
 * Copyright (c) 2026 Babb Works. Licensed under MIT.
 */

#ifndef LEDGERD_STORE_H
#define LEDGERD_STORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declare sqlite3 to avoid including sqlite3.h in header */
typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;

/* ========================================================================
 * CONSTANTS
 * ======================================================================== */

#define LDS_HASH_LEN   32u  /* BLAKE3 hash output size */
#define LDS_SIG_LEN    64u  /* ed25519 signature size */
#define LDS_MAX_CHAINS 16u  /* Maximum concurrent chains (Islands) */

/* Error codes */
#define LDS_OK             0
#define LDS_ERR_NULL      (-1)
#define LDS_ERR_DB        (-2)  /* SQLite operation failed */
#define LDS_ERR_CHAIN     (-3)  /* Chain hash mismatch (tamper detected) */
#define LDS_ERR_NOT_FOUND (-4)  /* Record or chain not found */
#define LDS_ERR_FULL      (-5)  /* Batch not open, chain table full, or overflow */
#define LDS_ERR_EXISTS    (-6)  /* Chain name already exists */
#define LDS_ERR_SIG       (-7)  /* Signature verification failed */

/* ========================================================================
 * STRUCTURES
 * ======================================================================== */

/* Per-chain in-memory state (cached tip for fast append) */
typedef struct {
    int64_t  chain_id;
    int64_t  tip_seq;                      /* Sequence of most recent record on this chain */
    uint8_t  tip_hash[LDS_HASH_LEN];      /* Chain hash of most recent record */
    uint8_t  active;                       /* 1 if this slot is in use */
} lds_chain_tip_t;

/* Handle to the store */
typedef struct {
    sqlite3 *db;

    /* Per-chain tip cache */
    lds_chain_tip_t chain_tips[LDS_MAX_CHAINS];

    /* Global sequence counter (across all chains) */
    int64_t  last_seq;

    /* Batch state */
    int64_t  current_batch_id;      /* Active batch ID (-1 if none) */
    int64_t  current_batch_chain;   /* Chain the batch belongs to */
    int64_t  batch_balance;         /* Running sum for conservation */

    /* Cached prepared statements */
    sqlite3_stmt *stmt_insert;
    sqlite3_stmt *stmt_batch_update;
    sqlite3_stmt *stmt_get_by_seq;
    sqlite3_stmt *stmt_get_by_hash;
    sqlite3_stmt *stmt_verify_range;
} lds_store_t;

/* Chain (Island) info */
typedef struct {
    int64_t  id;
    char     name[64];
    uint8_t  genesis_hash[LDS_HASH_LEN];
    uint8_t  tip_hash[LDS_HASH_LEN];
    int64_t  tip_seq;
    int64_t  created;
} lds_chain_t;

/* Record as stored/retrieved */
typedef struct {
    int64_t  seq;                       /* Sequence number (1-based, global) */
    int64_t  chain_id;                  /* Which chain (Island) this belongs to */
    uint8_t  frame[64];                 /* Raw BitLedger frame bytes */
    size_t   frame_len;                 /* Actual frame length */
    uint8_t  frame_hash[LDS_HASH_LEN]; /* BLAKE3 hash of frame */
    uint8_t  chain_hash[LDS_HASH_LEN]; /* Chain link to predecessor */
    uint8_t  parent2[LDS_HASH_LEN];    /* Second parent (merge record), zeros if none */
    uint8_t  has_parent2;               /* 1 if this is a merge (bilateral settlement) */
    uint32_t sender_id;                 /* From Layer 1 */
    int64_t  batch_id;                  /* Batch this record belongs to */
    uint8_t  direction;                 /* 0=In, 1=Out */
    uint32_t value_n;                   /* 25-bit value */
    int64_t  timestamp;                 /* Unix epoch seconds */
    uint8_t  signature[LDS_SIG_LEN];   /* ed25519 signature (zeros if unsigned) */
    uint8_t  has_signature;             /* 1 if signed */
    uint8_t  packed;                    /* 1 if frame blob moved to pack */
} lds_record_t;

/* Batch info */
typedef struct {
    int64_t  id;
    int64_t  chain_id;
    int64_t  opened;        /* Unix timestamp */
    int64_t  closed;        /* 0 if still open */
    int64_t  record_count;
    int64_t  balance;       /* Net balance (should be 0 when conserved) */
    int      conserved;     /* 1 if batch closed with balance=0 */
} lds_batch_t;

/* Pack info */
typedef struct {
    int64_t  id;
    int64_t  chain_id;
    int64_t  start_seq;
    int64_t  end_seq;
    int64_t  record_count;
    int64_t  created;
} lds_pack_info_t;

/* ========================================================================
 * PUBLIC API — STORE LIFECYCLE
 * ======================================================================== */

int lds_open(lds_store_t *store, const char *db_path);
void lds_close(lds_store_t *store);

/* ========================================================================
 * PUBLIC API — CHAIN (ISLAND) MANAGEMENT
 * ======================================================================== */

/*
 * lds_chain_create — Create a new chain (Island).
 *
 * Each chain has its own genesis anchor and independent record sequence.
 * The chain tip is cached in memory for fast append.
 *
 * @param store     Open store
 * @param name      Chain name (e.g. "personal", "work", "power")
 * @param out_id    Output: chain ID
 * @return LDS_OK, LDS_ERR_EXISTS, or LDS_ERR_FULL
 */
int lds_chain_create(lds_store_t *store, const char *name, int64_t *out_id);

/*
 * lds_chain_get — Get chain info by name.
 */
int lds_chain_get(lds_store_t *store, const char *name, lds_chain_t *out);

/*
 * lds_chain_get_by_id — Get chain info by ID.
 */
int lds_chain_get_by_id(lds_store_t *store, int64_t chain_id, lds_chain_t *out);

/*
 * lds_chain_list — List all chains.
 *
 * @param store      Open store
 * @param out        Output array
 * @param max_chains Max entries
 * @param out_count  Output: number written
 * @return LDS_OK
 */
int lds_chain_list(lds_store_t *store, lds_chain_t *out,
                   size_t max_chains, size_t *out_count);

/* ========================================================================
 * PUBLIC API — RECORD APPEND
 * ======================================================================== */

/*
 * lds_append — Append a record to a specific chain.
 *
 * Computes frame_hash and chain_hash against the chain's tip.
 * Uses cached prepared statement (no per-call SQL compilation).
 *
 * @param store      Open store
 * @param chain_id   Target chain (Island)
 * @param frame      Raw BitLedger frame bytes
 * @param frame_len  Frame length
 * @param sender_id  Sender identity (from Layer 1)
 * @param direction  0=In, 1=Out
 * @param value_n    25-bit value from Layer 3
 * @param sig        Optional ed25519 signature over chain_hash (NULL if unsigned)
 * @param parent2    Optional second parent chain_hash for merge records (NULL if normal)
 * @param out_seq    Output: global sequence number assigned
 * @return LDS_OK on success
 */
int lds_append(lds_store_t *store, int64_t chain_id,
               const uint8_t *frame, size_t frame_len,
               uint32_t sender_id, uint8_t direction,
               uint32_t value_n,
               const uint8_t *sig,
               const uint8_t *parent2,
               int64_t *out_seq);

/* ========================================================================
 * PUBLIC API — RECORD RETRIEVAL
 * ======================================================================== */

int lds_get_by_seq(lds_store_t *store, int64_t seq, lds_record_t *out);

int lds_get_by_hash(lds_store_t *store,
                    const uint8_t hash[LDS_HASH_LEN],
                    lds_record_t *out);

/* ========================================================================
 * PUBLIC API — CHAIN VERIFICATION
 * ======================================================================== */

/*
 * lds_verify_chain — Verify chain integrity for a specific chain.
 *
 * Uses cursor-based sequential scan (single prepared statement)
 * instead of per-record queries. Optionally verifies signatures
 * on signed records.
 *
 * @param store       Open store
 * @param chain_id    Chain to verify
 * @param seq_start   Start sequence (inclusive)
 * @param seq_end     End sequence (inclusive)
 * @param verify_sigs 1 to also verify ed25519 signatures on signed records
 * @return LDS_OK if chain intact, LDS_ERR_CHAIN if tampered,
 *         LDS_ERR_SIG if signature invalid
 */
int lds_verify_chain(lds_store_t *store, int64_t chain_id,
                     int64_t seq_start, int64_t seq_end,
                     int verify_sigs);

/* ========================================================================
 * PUBLIC API — BATCH CONSERVATION
 * ======================================================================== */

int lds_batch_open(lds_store_t *store, int64_t chain_id, int64_t *out_id);
int lds_batch_close(lds_store_t *store, int64_t *out_balance);

/* ========================================================================
 * PUBLIC API — PACK COMPACTION
 * ======================================================================== */

/*
 * lds_pack_compact — Compact a range of records into a pack.
 *
 * Reads sequential records, delta-compresses frame blobs into a single
 * pack BLOB, writes the pack, and NULLs the frame column on packed rows.
 * Chain hashes and frame hashes are preserved for verification.
 *
 * Should only be called during FULL power mode.
 *
 * @param store      Open store
 * @param chain_id   Chain to compact
 * @param start_seq  First record to pack
 * @param end_seq    Last record to pack
 * @param out_pack   Output: pack info
 * @return LDS_OK on success
 */
int lds_pack_compact(lds_store_t *store, int64_t chain_id,
                     int64_t start_seq, int64_t end_seq,
                     lds_pack_info_t *out_pack);

/*
 * lds_pack_get_frame — Retrieve a frame from a pack by sequence number.
 *
 * Used when the records table has packed=1 and frame IS NULL.
 *
 * @param store      Open store
 * @param seq        Sequence number of the record
 * @param out_frame  Output buffer (64 bytes)
 * @param out_len    Output: actual frame length
 * @return LDS_OK on success
 */
int lds_pack_get_frame(lds_store_t *store, int64_t seq,
                       uint8_t out_frame[64], size_t *out_len);

/* ========================================================================
 * PUBLIC API — UTILITIES
 * ======================================================================== */

int64_t lds_get_last_seq(lds_store_t *store);
int lds_fsync(lds_store_t *store);

#ifdef __cplusplus
}
#endif

#endif /* LEDGERD_STORE_H */
