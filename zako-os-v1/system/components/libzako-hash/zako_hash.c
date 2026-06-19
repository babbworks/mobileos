/*
 * zako_hash.c — BLAKE3 chain hashing implementation for ZAKO OS
 *
 * Implementation notes:
 * - Uses the BLAKE3 reference C implementation (blake3.h / blake3.c)
 * - No dynamic allocation — all hashing uses stack-allocated hasher state
 * - Constant-time comparison for hash equality (timing side-channel protection)
 *
 * Copyright (c) 2026 Babb Works. Licensed under MIT.
 */

#include "zako_hash.h"
#include "blake3.h"

#include <string.h>

int zako_frame_hash(const uint8_t *record,
                    size_t record_len,
                    uint8_t out_hash[ZAKO_HASH_LEN])
{
    blake3_hasher hasher;

    if (record == NULL || out_hash == NULL) {
        return ZAKO_HASH_ERR_NULL;
    }
    if (record_len == 0u) {
        return ZAKO_HASH_ERR_SIZE;
    }

    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, record, record_len);
    blake3_hasher_finalize(&hasher, out_hash, ZAKO_HASH_LEN);

    return ZAKO_HASH_OK;
}

int zako_chain_hash(const uint8_t frame_hash[ZAKO_HASH_LEN],
                    const uint8_t prev_chain_hash[ZAKO_HASH_LEN],
                    uint8_t out_chain_hash[ZAKO_HASH_LEN])
{
    blake3_hasher hasher;

    if (frame_hash == NULL || prev_chain_hash == NULL || out_chain_hash == NULL) {
        return ZAKO_HASH_ERR_NULL;
    }

    /*
     * Chain construction: BLAKE3(frame_hash || prev_chain_hash)
     *
     * The concatenation order is deliberate:
     * - frame_hash first: the current record's identity
     * - prev_chain_hash second: the link to history
     *
     * This means the chain_hash depends on BOTH the current record
     * AND the entire history before it. Changing anything breaks everything after.
     */
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, frame_hash, ZAKO_HASH_LEN);
    blake3_hasher_update(&hasher, prev_chain_hash, ZAKO_HASH_LEN);
    blake3_hasher_finalize(&hasher, out_chain_hash, ZAKO_HASH_LEN);

    return ZAKO_HASH_OK;
}

int zako_genesis_anchor(const uint8_t genesis_frame_hash[ZAKO_HASH_LEN],
                        uint8_t out_anchor[ZAKO_HASH_LEN])
{
    /* 32 zero bytes — the "nothing came before" sentinel */
    static const uint8_t zeros[ZAKO_HASH_LEN] = {0};

    if (genesis_frame_hash == NULL || out_anchor == NULL) {
        return ZAKO_HASH_ERR_NULL;
    }

    /*
     * Genesis anchor: BLAKE3(frame_hash[0] || zeros_32)
     *
     * This is just chain_hash() where prev_chain_hash is all zeros.
     * Factored through chain_hash for consistency — but inlined here
     * to avoid the function call overhead in a hot path and to make
     * the zero-sentinel explicit in the code.
     */
    return zako_chain_hash(genesis_frame_hash, zeros, out_anchor);
}

int zako_hash_equal(const uint8_t hash_a[ZAKO_HASH_LEN],
                    const uint8_t hash_b[ZAKO_HASH_LEN])
{
    volatile uint8_t diff = 0u;
    size_t i;

    if (hash_a == NULL || hash_b == NULL) {
        return 0;
    }

    /*
     * Constant-time comparison.
     *
     * We XOR every byte pair and OR the results together. If any byte
     * differs, diff becomes non-zero. The loop always executes all
     * iterations regardless of where a difference occurs — no early exit.
     *
     * The 'volatile' qualifier prevents the compiler from optimizing
     * this into a short-circuit comparison.
     */
    for (i = 0u; i < ZAKO_HASH_LEN; i++) {
        diff |= (hash_a[i] ^ hash_b[i]);
    }

    /* Return 1 if equal (diff == 0), 0 if not */
    return (diff == 0u) ? 1 : 0;
}
