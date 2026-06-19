/*
 * zako_sign.h — ed25519 signing for ZAKO OS
 *
 * This library provides the signing operations that establish sovereignty
 * in ZAKO. Every record, every capability grant, every identity assertion
 * is signed by the entity that authored it using ed25519.
 *
 * Key algorithm: Ed25519 (EdDSA on Curve25519)
 * Key size: 32 bytes (public), 64 bytes (secret/expanded)
 * Signature size: 64 bytes
 * Security level: ~128 bits
 *
 * Implementation: wraps TweetNaCl (crypto_sign_ed25519)
 *
 * MISRA-C:2012 compliance notes:
 * - No dynamic allocation
 * - All buffers are caller-provided with explicit sizes
 * - Secret key material is zeroed after use where appropriate
 * - All functions return explicit error codes
 *
 * Copyright (c) 2026 Babb Works. Licensed under MIT.
 */

#ifndef ZAKO_SIGN_H
#define ZAKO_SIGN_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Key and signature sizes (bytes) */
#define ZAKO_SIGN_PUBKEY_LEN   32u   /* ed25519 public key */
#define ZAKO_SIGN_SECKEY_LEN   64u   /* ed25519 secret key (expanded: seed + public) */
#define ZAKO_SIGN_SEED_LEN     32u   /* ed25519 seed (the 32 bytes that generate a keypair) */
#define ZAKO_SIGN_SIG_LEN      64u   /* ed25519 signature */

/* Error codes */
#define ZAKO_SIGN_OK           0
#define ZAKO_SIGN_ERR_NULL     (-1)  /* NULL pointer argument */
#define ZAKO_SIGN_ERR_SIZE     (-2)  /* Invalid size argument */
#define ZAKO_SIGN_ERR_VERIFY   (-3)  /* Signature verification failed */
#define ZAKO_SIGN_ERR_KEYGEN   (-4)  /* Key generation failed (RNG failure) */

/*
 * zako_sign_keypair — Generate a new ed25519 keypair.
 *
 * Generates a random seed, derives the keypair from it. The seed is
 * sourced from the OS random device (/dev/urandom on Linux).
 *
 * On the target device with TrustZone, this function would be replaced
 * by a call to the Keymaster HAL. For development and testing, the
 * software implementation is used.
 *
 * @param out_pubkey  Output: public key (ZAKO_SIGN_PUBKEY_LEN bytes)
 * @param out_seckey  Output: secret key (ZAKO_SIGN_SECKEY_LEN bytes)
 *                    SECURITY: caller must protect this buffer and zero it when done
 *
 * @return ZAKO_SIGN_OK on success, ZAKO_SIGN_ERR_KEYGEN on RNG failure
 */
int zako_sign_keypair(uint8_t out_pubkey[ZAKO_SIGN_PUBKEY_LEN],
                      uint8_t out_seckey[ZAKO_SIGN_SECKEY_LEN]);

/*
 * zako_sign_keypair_from_seed — Derive a keypair from a known seed.
 *
 * Deterministic: same seed always produces the same keypair.
 * Used for testing (known test vectors) and for key recovery from
 * stored seed material.
 *
 * @param seed        Input: 32-byte seed
 * @param out_pubkey  Output: public key (ZAKO_SIGN_PUBKEY_LEN bytes)
 * @param out_seckey  Output: secret key (ZAKO_SIGN_SECKEY_LEN bytes)
 *
 * @return ZAKO_SIGN_OK on success, ZAKO_SIGN_ERR_NULL on NULL input
 */
int zako_sign_keypair_from_seed(const uint8_t seed[ZAKO_SIGN_SEED_LEN],
                                uint8_t out_pubkey[ZAKO_SIGN_PUBKEY_LEN],
                                uint8_t out_seckey[ZAKO_SIGN_SECKEY_LEN]);

/*
 * zako_sign — Sign a message with a secret key.
 *
 * Produces a 64-byte ed25519 signature over the message.
 * The signature is deterministic: signing the same message with the
 * same key always produces the same signature. No nonce randomness needed.
 *
 * @param message     The message bytes to sign
 * @param message_len Length of the message (must be > 0)
 * @param seckey      The signer's secret key (ZAKO_SIGN_SECKEY_LEN bytes)
 * @param out_sig     Output: signature (ZAKO_SIGN_SIG_LEN bytes)
 *
 * @return ZAKO_SIGN_OK on success, negative error code on failure
 */
int zako_sign(const uint8_t *message,
              size_t message_len,
              const uint8_t seckey[ZAKO_SIGN_SECKEY_LEN],
              uint8_t out_sig[ZAKO_SIGN_SIG_LEN]);

/*
 * zako_sign_verify — Verify a signature against a public key.
 *
 * Returns ZAKO_SIGN_OK if the signature is valid for the given message
 * and public key. Returns ZAKO_SIGN_ERR_VERIFY if it is not.
 *
 * This is the operation that makes sovereignty verifiable: anyone with
 * the public key can verify that a record was signed by the Sovereign,
 * without needing the Sovereign's secret key or any online authority.
 *
 * @param message     The message bytes that were signed
 * @param message_len Length of the message
 * @param sig         The signature to verify (ZAKO_SIGN_SIG_LEN bytes)
 * @param pubkey      The signer's public key (ZAKO_SIGN_PUBKEY_LEN bytes)
 *
 * @return ZAKO_SIGN_OK if valid, ZAKO_SIGN_ERR_VERIFY if invalid
 */
int zako_sign_verify(const uint8_t *message,
                     size_t message_len,
                     const uint8_t sig[ZAKO_SIGN_SIG_LEN],
                     const uint8_t pubkey[ZAKO_SIGN_PUBKEY_LEN]);

/*
 * zako_sign_seckey_zero — Securely zero a secret key buffer.
 *
 * Uses volatile writes to prevent compiler optimization from removing
 * the zeroing operation. Call this when a secret key is no longer needed.
 *
 * @param seckey  Buffer to zero (ZAKO_SIGN_SECKEY_LEN bytes)
 */
void zako_sign_seckey_zero(uint8_t seckey[ZAKO_SIGN_SECKEY_LEN]);

#ifdef __cplusplus
}
#endif

#endif /* ZAKO_SIGN_H */
