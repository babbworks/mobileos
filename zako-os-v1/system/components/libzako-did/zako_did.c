/*
 * zako_did.c — W3C DID formatter implementation for ZAKO OS
 *
 * Implements did:key method with ed25519 multicodec prefix and base58btc encoding.
 *
 * Base58btc alphabet: 123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz
 * (Bitcoin alphabet — excludes 0, O, I, l to avoid visual ambiguity)
 *
 * Copyright (c) 2026 Babb Works. Licensed under MIT.
 */

#include "zako_did.h"
#include <string.h>

/* did:key prefix string */
static const char DID_PREFIX[] = "did:key:z";
#define DID_PREFIX_LEN 9u

/* Multicodec ed25519-pub prefix: 0xed 0x01 */
static const uint8_t MULTICODEC_ED25519[ZAKO_DID_MULTICODEC_LEN] = {0xed, 0x01};

/* Base58btc alphabet (Bitcoin) */
static const char B58_ALPHABET[] =
    "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

/* Reverse lookup table for base58 decoding */
static const int8_t B58_DECODE[128] = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1, 0, 1, 2, 3, 4, 5, 6, 7, 8,-1,-1,-1,-1,-1,-1,  /* '1'..'9' = 0..8 */
    -1, 9,10,11,12,13,14,15,16,-1,17,18,19,20,21,-1,  /* 'A'..'N' (skip I) */
    22,23,24,25,26,27,28,29,30,31,32,-1,-1,-1,-1,-1,  /* 'P'..'Z' */
    -1,33,34,35,36,37,38,39,40,41,42,43,-1,44,45,46,  /* 'a'..'m' (skip l) */
    47,48,49,50,51,52,53,54,55,56,57,-1,-1,-1,-1,-1   /* 'n'..'z' */
};

/*
 * base58btc_encode — Encode binary data to base58btc string.
 *
 * Standard big-endian base conversion algorithm.
 * No dynamic allocation — uses caller-provided output buffer.
 */
static int base58btc_encode(const uint8_t *data, size_t data_len,
                            char *out, size_t out_max, size_t *out_written)
{
    /* Temporary buffer for base conversion (big enough for 34 input bytes) */
    uint8_t buf[64] = {0};
    size_t buf_len = 0;
    size_t i, j;
    int carry;

    if (data == NULL || out == NULL || out_written == NULL) {
        return ZAKO_DID_ERR_NULL;
    }

    /* Count leading zero bytes (they become '1' characters in base58) */
    size_t leading_zeros = 0;
    while (leading_zeros < data_len && data[leading_zeros] == 0u) {
        leading_zeros++;
    }

    /* Base conversion: repeatedly divide by 58 */
    for (i = leading_zeros; i < data_len; i++) {
        carry = (int)data[i];
        for (j = 0; j < buf_len; j++) {
            carry += (int)buf[j] * 256;
            buf[j] = (uint8_t)(carry % 58);
            carry /= 58;
        }
        while (carry > 0) {
            if (buf_len >= sizeof(buf)) {
                return ZAKO_DID_ERR_SIZE;
            }
            buf[buf_len] = (uint8_t)(carry % 58);
            carry /= 58;
            buf_len++;
        }
    }

    /* Check output buffer has enough space */
    size_t total_len = leading_zeros + buf_len;
    if (total_len + 1u > out_max) {
        return ZAKO_DID_ERR_SIZE;
    }

    /* Write leading '1' chars for zero bytes */
    for (i = 0; i < leading_zeros; i++) {
        out[i] = '1';
    }

    /* Write the base58 digits in reverse order */
    for (i = 0; i < buf_len; i++) {
        out[leading_zeros + i] = B58_ALPHABET[buf[buf_len - 1u - i]];
    }

    out[total_len] = '\0';
    *out_written = total_len;
    return ZAKO_DID_OK;
}

/*
 * base58btc_decode — Decode base58btc string to binary data.
 */
static int base58btc_decode(const char *str, size_t str_len,
                            uint8_t *out, size_t out_max, size_t *out_written)
{
    uint8_t buf[64] = {0};
    size_t buf_len = 0;
    size_t i, j;
    int carry;

    if (str == NULL || out == NULL || out_written == NULL) {
        return ZAKO_DID_ERR_NULL;
    }

    /* Count leading '1' characters (they represent zero bytes) */
    size_t leading_ones = 0;
    while (leading_ones < str_len && str[leading_ones] == '1') {
        leading_ones++;
    }

    /* Base conversion: from base58 to base256 */
    for (i = leading_ones; i < str_len; i++) {
        unsigned char ch = (unsigned char)str[i];
        if (ch >= 128u || B58_DECODE[ch] < 0) {
            return ZAKO_DID_ERR_DECODE;
        }
        carry = (int)B58_DECODE[ch];
        for (j = 0; j < buf_len; j++) {
            carry += (int)buf[j] * 58;
            buf[j] = (uint8_t)(carry % 256);
            carry /= 256;
        }
        while (carry > 0) {
            if (buf_len >= sizeof(buf)) {
                return ZAKO_DID_ERR_SIZE;
            }
            buf[buf_len] = (uint8_t)(carry % 256);
            carry /= 256;
            buf_len++;
        }
    }

    /* Total output length */
    size_t total_len = leading_ones + buf_len;
    if (total_len > out_max) {
        return ZAKO_DID_ERR_SIZE;
    }

    /* Write leading zero bytes */
    for (i = 0; i < leading_ones; i++) {
        out[i] = 0u;
    }

    /* Write decoded bytes in reverse */
    for (i = 0; i < buf_len; i++) {
        out[leading_ones + i] = buf[buf_len - 1u - i];
    }

    *out_written = total_len;
    return ZAKO_DID_OK;
}

/* ---- Public API ---- */

int zako_did_from_pubkey(const uint8_t pubkey[ZAKO_DID_PUBKEY_LEN],
                         char *out_did,
                         size_t out_len)
{
    uint8_t raw[ZAKO_DID_RAW_LEN];
    char b58_buf[56]; /* base58 of 34 bytes ≈ 47 chars + null */
    size_t b58_len = 0;
    int rc;

    if (pubkey == NULL || out_did == NULL) {
        return ZAKO_DID_ERR_NULL;
    }
    if (out_len < ZAKO_DID_STR_MAX) {
        return ZAKO_DID_ERR_SIZE;
    }

    /* Assemble raw bytes: multicodec prefix + public key */
    memcpy(raw, MULTICODEC_ED25519, ZAKO_DID_MULTICODEC_LEN);
    memcpy(raw + ZAKO_DID_MULTICODEC_LEN, pubkey, ZAKO_DID_PUBKEY_LEN);

    /* Base58btc encode */
    rc = base58btc_encode(raw, ZAKO_DID_RAW_LEN, b58_buf, sizeof(b58_buf), &b58_len);
    if (rc != ZAKO_DID_OK) {
        return rc;
    }

    /* Assemble DID string: "did:key:z" + base58 encoded bytes */
    if (DID_PREFIX_LEN + b58_len + 1u > out_len) {
        return ZAKO_DID_ERR_SIZE;
    }

    memcpy(out_did, DID_PREFIX, DID_PREFIX_LEN);
    memcpy(out_did + DID_PREFIX_LEN, b58_buf, b58_len);
    out_did[DID_PREFIX_LEN + b58_len] = '\0';

    return ZAKO_DID_OK;
}

int zako_did_to_pubkey(const char *did_str,
                       uint8_t out_pubkey[ZAKO_DID_PUBKEY_LEN])
{
    size_t did_len;
    uint8_t decoded[64];
    size_t decoded_len = 0;
    int rc;

    if (did_str == NULL || out_pubkey == NULL) {
        return ZAKO_DID_ERR_NULL;
    }

    did_len = strlen(did_str);

    /* Verify "did:key:z" prefix */
    if (did_len <= DID_PREFIX_LEN) {
        return ZAKO_DID_ERR_FORMAT;
    }
    if (memcmp(did_str, DID_PREFIX, DID_PREFIX_LEN) != 0) {
        return ZAKO_DID_ERR_FORMAT;
    }

    /* Decode the base58btc portion (everything after "did:key:z") */
    const char *b58_part = did_str + DID_PREFIX_LEN;
    size_t b58_len = did_len - DID_PREFIX_LEN;

    rc = base58btc_decode(b58_part, b58_len, decoded, sizeof(decoded), &decoded_len);
    if (rc != ZAKO_DID_OK) {
        return rc;
    }

    /* Verify decoded length: should be multicodec(2) + pubkey(32) = 34 */
    if (decoded_len != ZAKO_DID_RAW_LEN) {
        return ZAKO_DID_ERR_FORMAT;
    }

    /* Verify multicodec prefix (0xed 0x01 = ed25519-pub) */
    if (decoded[0] != 0xed || decoded[1] != 0x01) {
        return ZAKO_DID_ERR_FORMAT;
    }

    /* Extract public key (bytes 2..33) */
    memcpy(out_pubkey, decoded + ZAKO_DID_MULTICODEC_LEN, ZAKO_DID_PUBKEY_LEN);

    return ZAKO_DID_OK;
}

int zako_did_is_valid(const char *did_str)
{
    uint8_t pubkey[ZAKO_DID_PUBKEY_LEN];

    if (did_str == NULL) {
        return 0;
    }

    /* Attempt full parse — if it succeeds, the DID is valid */
    return (zako_did_to_pubkey(did_str, pubkey) == ZAKO_DID_OK) ? 1 : 0;
}
