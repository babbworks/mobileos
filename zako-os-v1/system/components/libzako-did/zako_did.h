/*
 * zako_did.h — W3C Decentralized Identifier (DID) formatter for ZAKO OS
 *
 * Implements the did:key method for ed25519 public keys.
 * Format: did:key:z6Mk<BASE58BTC(0xed01 || pubkey)>
 *
 * The DID is the identity. The identity is the key. No external resolution
 * needed. Anyone with the DID string can extract the public key and verify
 * signatures. This is self-sovereign identity at its purest.
 *
 * Encoding chain:
 *   pubkey (32 bytes)
 *   → prepend multicodec prefix 0xed 0x01 (ed25519-pub)
 *   → 34 bytes total
 *   → base58btc encode
 *   → prepend "z" (multibase prefix for base58btc)
 *   → prepend "did:key:"
 *   → final DID string
 *
 * MISRA-C:2012 compliance:
 * - No dynamic allocation (caller provides all buffers)
 * - All functions return explicit error codes
 * - Fixed-size buffers with compile-time known maximums
 *
 * Copyright (c) 2026 Babb Works. Licensed under MIT.
 */

#ifndef ZAKO_DID_H
#define ZAKO_DID_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ed25519 public key size */
#define ZAKO_DID_PUBKEY_LEN    32u

/* Multicodec prefix for ed25519-pub: 0xed 0x01 (varint-encoded 0xed01) */
#define ZAKO_DID_MULTICODEC_LEN 2u

/* Raw bytes before base58 encoding: multicodec(2) + pubkey(32) = 34 */
#define ZAKO_DID_RAW_LEN       34u

/* Maximum DID string length:
 * "did:key:z" (9 chars) + base58btc(34 bytes) ≈ 48 chars + null = 58 max
 * Base58 expansion: ceil(34 * log(256) / log(58)) ≈ 47 chars
 */
#define ZAKO_DID_STR_MAX       64u

/* Error codes */
#define ZAKO_DID_OK            0
#define ZAKO_DID_ERR_NULL      (-1)
#define ZAKO_DID_ERR_FORMAT    (-2)  /* DID string doesn't match expected format */
#define ZAKO_DID_ERR_DECODE    (-3)  /* Base58 decoding failed */
#define ZAKO_DID_ERR_SIZE      (-4)  /* Buffer too small */

/*
 * zako_did_from_pubkey — Format an ed25519 public key as a did:key DID.
 *
 * Produces a string like: did:key:z6MkhaXgBZDvotDkL5257faiztiGiC2QtKLGpb...
 *
 * @param pubkey    Input: ed25519 public key (ZAKO_DID_PUBKEY_LEN bytes)
 * @param out_did   Output: null-terminated DID string buffer
 * @param out_len   Size of out_did buffer (must be >= ZAKO_DID_STR_MAX)
 *
 * @return ZAKO_DID_OK on success, negative error code on failure
 */
int zako_did_from_pubkey(const uint8_t pubkey[ZAKO_DID_PUBKEY_LEN],
                         char *out_did,
                         size_t out_len);

/*
 * zako_did_to_pubkey — Extract the ed25519 public key from a did:key DID.
 *
 * Parses the DID string, verifies the did:key:z prefix, decodes base58btc,
 * verifies the ed25519 multicodec prefix, and extracts the 32-byte public key.
 *
 * @param did_str       Input: null-terminated DID string
 * @param out_pubkey    Output: extracted public key (ZAKO_DID_PUBKEY_LEN bytes)
 *
 * @return ZAKO_DID_OK on success, ZAKO_DID_ERR_FORMAT if prefix wrong,
 *         ZAKO_DID_ERR_DECODE if base58 decode fails
 */
int zako_did_to_pubkey(const char *did_str,
                       uint8_t out_pubkey[ZAKO_DID_PUBKEY_LEN]);

/*
 * zako_did_is_valid — Check if a string is a syntactically valid did:key DID.
 *
 * Verifies prefix, attempts decode, checks multicodec. Does NOT verify
 * that the key is on the curve (that's a signing library concern).
 *
 * @param did_str   Input: null-terminated string to check
 *
 * @return 1 if valid did:key format, 0 if not
 */
int zako_did_is_valid(const char *did_str);

#ifdef __cplusplus
}
#endif

#endif /* ZAKO_DID_H */
