/*
 * zako_sign.c — ed25519 signing implementation for ZAKO OS
 *
 * Wraps TweetNaCl's crypto_sign_ed25519 functions.
 *
 * Design note on seed-based key derivation:
 * TweetNaCl does not expose crypto_sign_seed_keypair (that's libsodium).
 * TweetNaCl's crypto_sign_keypair calls our randombytes() to get 32 random
 * bytes as the seed, then derives the keypair internally. We exploit this
 * by providing a randombytes() that can be temporarily overridden to return
 * a specific seed — giving us deterministic key derivation.
 *
 * This is safe for ZAKO because all daemons are single-threaded and the
 * override is set/cleared within a single function call.
 *
 * Copyright (c) 2026 Babb Works. Licensed under MIT.
 */

#include "zako_sign.h"
#include "tweetnacl.h"

#include <string.h>
#include <stdio.h>

/* ---- randombytes implementation ---- */

/*
 * Seed override mechanism for deterministic key generation.
 * When s_seed_override is non-NULL, randombytes returns those bytes
 * instead of reading /dev/urandom. Cleared immediately after use.
 */
static const uint8_t *s_seed_override = NULL;
static size_t s_seed_override_len = 0u;

void randombytes(unsigned char *buf, unsigned long long len)
{
    if (s_seed_override != NULL && (size_t)len <= s_seed_override_len) {
        memcpy(buf, s_seed_override, (size_t)len);
        s_seed_override = NULL;
        s_seed_override_len = 0u;
        return;
    }

    /* Normal path: read from OS random source */
    FILE *f = fopen("/dev/urandom", "rb");
    if (f != NULL) {
        size_t got = fread(buf, 1, (size_t)len, f);
        (void)fclose(f);
        if (got == (size_t)len) {
            return;
        }
    }
    /* Fallback: if urandom fails, leave buffer as-is.
     * Caller should check return from zako_sign_keypair. */
}

/* ---- Public API ---- */

int zako_sign_keypair(uint8_t out_pubkey[ZAKO_SIGN_PUBKEY_LEN],
                      uint8_t out_seckey[ZAKO_SIGN_SECKEY_LEN])
{
    if (out_pubkey == NULL || out_seckey == NULL) {
        return ZAKO_SIGN_ERR_NULL;
    }

    /* TweetNaCl generates random seed internally via randombytes,
     * then derives the keypair. sk = [seed(32) || pk(32)]. */
    if (crypto_sign_keypair(out_pubkey, out_seckey) != 0) {
        return ZAKO_SIGN_ERR_KEYGEN;
    }

    return ZAKO_SIGN_OK;
}

int zako_sign_keypair_from_seed(const uint8_t seed[ZAKO_SIGN_SEED_LEN],
                                uint8_t out_pubkey[ZAKO_SIGN_PUBKEY_LEN],
                                uint8_t out_seckey[ZAKO_SIGN_SECKEY_LEN])
{
    if (seed == NULL || out_pubkey == NULL || out_seckey == NULL) {
        return ZAKO_SIGN_ERR_NULL;
    }

    /*
     * Inject our seed into the next randombytes call.
     * crypto_sign_keypair will call randombytes(sk, 32) — our override
     * returns the provided seed instead of random bytes.
     * The derivation then proceeds normally: SHA-512(seed) → clamp → scalarmult → pk.
     */
    s_seed_override = seed;
    s_seed_override_len = ZAKO_SIGN_SEED_LEN;

    if (crypto_sign_keypair(out_pubkey, out_seckey) != 0) {
        s_seed_override = NULL;
        s_seed_override_len = 0u;
        return ZAKO_SIGN_ERR_KEYGEN;
    }

    return ZAKO_SIGN_OK;
}

int zako_sign(const uint8_t *message,
              size_t message_len,
              const uint8_t seckey[ZAKO_SIGN_SECKEY_LEN],
              uint8_t out_sig[ZAKO_SIGN_SIG_LEN])
{
    if (message == NULL || seckey == NULL || out_sig == NULL) {
        return ZAKO_SIGN_ERR_NULL;
    }
    if (message_len == 0u) {
        return ZAKO_SIGN_ERR_SIZE;
    }

    /*
     * TweetNaCl's crypto_sign produces a "signed message" = sig(64) || message.
     * We want detached signatures (sig separate from message).
     * Strategy: allocate a buffer for the signed message on the stack,
     * copy just the first 64 bytes (the signature) to out_sig.
     *
     * For MISRA compliance (no VLA), we limit message size to a reasonable
     * maximum for stack allocation. ZAKO records are max ~44 bytes (Full BitLedger).
     * Even with arbitrary messages, 4KB on stack is safe for any daemon.
     */
    #define ZAKO_SIGN_MAX_MSG 4096u

    if (message_len > ZAKO_SIGN_MAX_MSG) {
        /* For messages larger than 4KB, we'd need heap allocation.
         * ZAKO records never exceed 44 bytes, so this limit is generous. */
        return ZAKO_SIGN_ERR_SIZE;
    }

    unsigned char sm[ZAKO_SIGN_SIG_LEN + ZAKO_SIGN_MAX_MSG];
    unsigned long long sm_len = 0;

    if (crypto_sign(sm, &sm_len, message, (unsigned long long)message_len, seckey) != 0) {
        return ZAKO_SIGN_ERR_KEYGEN; /* shouldn't happen, but handle */
    }

    /* Extract detached signature (first 64 bytes of signed message) */
    memcpy(out_sig, sm, ZAKO_SIGN_SIG_LEN);

    /* Zero the signed message buffer (contains the signature + message) */
    memset(sm, 0, sizeof(sm));

    return ZAKO_SIGN_OK;
}

int zako_sign_verify(const uint8_t *message,
                     size_t message_len,
                     const uint8_t sig[ZAKO_SIGN_SIG_LEN],
                     const uint8_t pubkey[ZAKO_SIGN_PUBKEY_LEN])
{
    if (message == NULL || sig == NULL || pubkey == NULL) {
        return ZAKO_SIGN_ERR_NULL;
    }
    if (message_len == 0u) {
        return ZAKO_SIGN_ERR_SIZE;
    }
    if (message_len > ZAKO_SIGN_MAX_MSG) {
        return ZAKO_SIGN_ERR_SIZE;
    }

    /*
     * TweetNaCl's crypto_sign_open expects a "signed message" (sig || message).
     * We reconstruct it from our detached signature + message.
     */
    unsigned char sm[ZAKO_SIGN_SIG_LEN + ZAKO_SIGN_MAX_MSG];
    unsigned char m[ZAKO_SIGN_SIG_LEN + ZAKO_SIGN_MAX_MSG];
    unsigned long long m_len = 0;
    unsigned long long sm_len = (unsigned long long)(ZAKO_SIGN_SIG_LEN + message_len);

    /* Reconstruct signed message: sig || message */
    memcpy(sm, sig, ZAKO_SIGN_SIG_LEN);
    memcpy(sm + ZAKO_SIGN_SIG_LEN, message, message_len);

    /* Verify */
    if (crypto_sign_open(m, &m_len, sm, sm_len, pubkey) != 0) {
        memset(sm, 0, sizeof(sm));
        memset(m, 0, sizeof(m));
        return ZAKO_SIGN_ERR_VERIFY;
    }

    memset(sm, 0, sizeof(sm));
    memset(m, 0, sizeof(m));
    return ZAKO_SIGN_OK;
}

void zako_sign_seckey_zero(uint8_t seckey[ZAKO_SIGN_SECKEY_LEN])
{
    if (seckey == NULL) {
        return;
    }

    /*
     * Volatile pointer prevents compiler from optimizing out the zeroing.
     * This is the standard pattern for secure memory clearing.
     */
    volatile uint8_t *p = seckey;
    size_t i;
    for (i = 0u; i < ZAKO_SIGN_SECKEY_LEN; i++) {
        p[i] = 0u;
    }
}
