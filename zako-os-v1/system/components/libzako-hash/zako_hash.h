/*
 * zako_hash.h — BLAKE3 chain hashing for ZAKO OS
 *
 * This library provides the three hash operations that underpin ZAKO's
 * tamper-evident record chain. Every record in the system is identified
 * by its frame_hash and linked to its predecessor via chain_hash.
 *
 * Hash algorithm: BLAKE3 (256-bit output)
 * Chain construction: chain_hash[n] = BLAKE3(frame_hash[n] || chain_hash[n-1])
 * Genesis anchor: BLAKE3(frame_hash[0] || zeros_32)
 *
 * MISRA-C:2012 compliance notes:
 * - No dynamic allocation
 * - No recursion
 * - All buffers are caller-provided with explicit sizes
 * - All functions return explicit error codes
 *
 * Copyright (c) 2026 Babb Works. Licensed under MIT.
 */

#ifndef ZAKO_HASH_H
#define ZAKO_HASH_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Hash output size in bytes (BLAKE3 produces 256-bit = 32-byte digests) */
#define ZAKO_HASH_LEN 32u

/* Error codes */
#define ZAKO_HASH_OK        0
#define ZAKO_HASH_ERR_NULL  (-1)  /* NULL pointer argument */
#define ZAKO_HASH_ERR_SIZE  (-2)  /* Invalid size argument */

/*
 * zako_frame_hash — Compute the fingerprint of a single record.
 *
 * This is a plain BLAKE3 hash of the record bytes. It uniquely identifies
 * the record content. Two records with identical bytes produce identical
 * frame hashes.
 *
 * @param record     Pointer to the record bytes (BitPads frame)
 * @param record_len Length of the record in bytes (must be > 0)
 * @param out_hash   Output buffer, must be at least ZAKO_HASH_LEN bytes
 *
 * @return ZAKO_HASH_OK on success, negative error code on failure
 */
int zako_frame_hash(const uint8_t *record,
                    size_t record_len,
                    uint8_t out_hash[ZAKO_HASH_LEN]);

/*
 * zako_chain_hash — Link a record to its predecessor in the chain.
 *
 * Computes: BLAKE3(frame_hash || prev_chain_hash)
 *
 * This creates an unbreakable forward chain: if any prior record is
 * modified, its frame_hash changes, which invalidates this chain_hash,
 * which invalidates every subsequent chain_hash in the sequence.
 *
 * @param frame_hash      The frame_hash of the current record (ZAKO_HASH_LEN bytes)
 * @param prev_chain_hash The chain_hash of the previous record (ZAKO_HASH_LEN bytes)
 * @param out_chain_hash  Output buffer, must be at least ZAKO_HASH_LEN bytes
 *
 * @return ZAKO_HASH_OK on success, negative error code on failure
 */
int zako_chain_hash(const uint8_t frame_hash[ZAKO_HASH_LEN],
                    const uint8_t prev_chain_hash[ZAKO_HASH_LEN],
                    uint8_t out_chain_hash[ZAKO_HASH_LEN]);

/*
 * zako_genesis_anchor — Create the starting point of a new chain.
 *
 * Computes: BLAKE3(frame_hash[0] || zeros_32)
 *
 * Every new chain (every new Island, every new ledger) starts from a
 * deterministic anchor. The genesis record's chain_hash is computed
 * against 32 zero bytes. This means anyone who has the genesis record
 * can independently verify the anchor — no external reference needed.
 *
 * @param genesis_frame_hash The frame_hash of the first record (ZAKO_HASH_LEN bytes)
 * @param out_anchor         Output buffer, must be at least ZAKO_HASH_LEN bytes
 *
 * @return ZAKO_HASH_OK on success, negative error code on failure
 */
int zako_genesis_anchor(const uint8_t genesis_frame_hash[ZAKO_HASH_LEN],
                        uint8_t out_anchor[ZAKO_HASH_LEN]);

/*
 * zako_hash_equal — Constant-time comparison of two hashes.
 *
 * Prevents timing side-channels when comparing hashes for verification.
 *
 * @param hash_a First hash (ZAKO_HASH_LEN bytes)
 * @param hash_b Second hash (ZAKO_HASH_LEN bytes)
 *
 * @return 1 if equal, 0 if not equal
 */
int zako_hash_equal(const uint8_t hash_a[ZAKO_HASH_LEN],
                    const uint8_t hash_b[ZAKO_HASH_LEN]);

#ifdef __cplusplus
}
#endif

#endif /* ZAKO_HASH_H */
